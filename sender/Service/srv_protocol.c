/**
 * @file srv_protocol.c
 * @brief 自定义无线通信协议打包、解包与流式状态机解析器实现
 */

#include "srv_protocol.h"
#include "system_config.h"
#include <string.h>

/**
 * @brief 以小端序 (Little-Endian) 写入 16 位无符号整数
 * @param[out] p 目标缓冲区指针 (至少 2 字节)
 * @param[in]  v 待写入的 16 位整数 (低字节在前，高字节在后)
 */
static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

/**
 * @brief 从小端序字节流中读取 16 位无符号整数
 * @param[in] p 源缓冲区指针 (至少 2 字节)
 * @return 还原出的 16 位无符号整数
 */
static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

/**
 * @brief 从小端序字节流中读取 16 位有符号整数 (安全处理二进制补码符号位扩展)
 * @param[in] p 源缓冲区指针 (至少 2 字节)
 * @return 还原出的 16 位有符号整数 (-32768 ~ +32767)
 */
static int16_t signed16(const uint8_t *p)
{
    uint16_t v = get16(p);
    return (int16_t)((v & 0x8000U) ? (int32_t)v - 65536 : (int32_t)v);
}

/**
 * @brief 协议层模块初始化 (静态无状态设计，无需额外硬件或内存开辟)
 */
void srv_protocol_init(void)
{
}

/**
 * @brief 计算指定数据段的 8 位累加和校验码
 * @param[in] p 数据指针
 * @param[in] n 数据长度 (字节数)
 * @return 8 位累加和 (溢出自动截断为 8 位)
 */
uint8_t srv_protocol_calc_checksum(const uint8_t *p, uint16_t n)
{
    uint8_t s = 0;
    if (p)
        while (n--)
            s = (uint8_t)(s + *p++);
    return s;
}

/**
 * @brief 写入通用的协议前导帧头 (0xAA 0x55 + SEQ + LEN + CMD)
 * @param[out] o   输出缓冲区
 * @param[in]  seq 序列号
 * @param[in]  len 载荷长度
 * @param[in]  cmd 命令功能码
 */
static void frame_header(uint8_t *o, uint8_t seq, uint8_t len, uint8_t cmd)
{
    o[0] = 0xAA;
    o[1] = 0x55;
    o[2] = seq;
    o[3] = len;
    o[4] = cmd;
}

/**
 * @brief 打包 23 字节遥测数据帧
 * @param[in]  seq 包序号 (0~255)
 * @param[in]  p   指向遥测核心数据结构体的指针
 * @param[out] o   输出缓冲区指针
 * @param[in]  cap 输出缓冲区可用容量 (必须 >= 23)
 * @return 实际写入的帧长度 (成功为 23，失败为 0)
 */
uint16_t srv_protocol_pack(uint8_t seq, const telemetry_payload_t *p, uint8_t *o, uint16_t cap)
{
    if (!p || !o || cap < 23)
        return 0;
    /* 写入帧头与元数据: 0xAA 0x55, seq, len=16, cmd=0x01 */
    frame_header(o, seq, 16, PROTOCOL_CMD_TELEMETRY);
    /* 编码载荷数据: 摇杆归一化坐标、采样电压、按键与拨码开关掩码、欧拉角 */
    put16(o + 5, (uint16_t)p->joy_x_raw);
    put16(o + 7, (uint16_t)p->joy_y_raw);
    put16(o + 9, p->joy_x_mv);
    put16(o + 11, p->joy_y_mv);
    o[13] = p->key_mask;
    o[14] = p->switch_mask;
    put16(o + 15, (uint16_t)p->pitch_cd);
    put16(o + 17, (uint16_t)p->roll_cd);
    put16(o + 19, (uint16_t)p->yaw_cd);
    /* 计算校验和: 对从 seq 开始到载荷末尾 (共 1 + 1 + 1 + 16 = 19 字节) 求和 */
    o[21] = srv_protocol_calc_checksum(o + 2, 19);
    /* 写入帧尾: 0x0D ('\r') */
    o[22] = 0x0D;
    return 23;
}

/**
 * @brief 解包遥测数据载荷
 * @param[in]  p 解包完成的原始协议包
 * @param[out] o 输出结构体
 * @return true 成功解析; false 格式不匹配
 */
bool srv_protocol_unpack(const protocol_packet_t *p, telemetry_payload_t *o)
{
    if (!p || !o || p->cmd != PROTOCOL_CMD_TELEMETRY || p->len != 16)
        return false;
    o->joy_x_raw = signed16(p->payload);
    o->joy_y_raw = signed16(p->payload + 2);
    o->joy_x_mv = get16(p->payload + 4);
    o->joy_y_mv = get16(p->payload + 6);
    o->key_mask = p->payload[8];
    o->switch_mask = p->payload[9];
    o->pitch_cd = signed16(p->payload + 10);
    o->roll_cd = signed16(p->payload + 12);
    o->yaw_cd = signed16(p->payload + 14);
    return true;
}

/**
 * @brief 计算全帧的 CRC-16/CCITT-FALSE 令牌
 * 
 * 用于在确认应答(ACK)中深度绑定接收到的遥测整帧(不仅基于 8 位 seq)，
 * 有效防止网络乱序或重放导致的历史 ACK 误匹配。
 * 
 * 多项式: 0x1021, 初值: 0xFFFF
 * 
 * @param[in] p 数据指针 (指向完整 23 字节帧)
 * @param[in] n 数据长度
 * @return 计算出的 16 位 CRC 令牌
 */
uint16_t srv_protocol_token(const uint8_t *p, uint16_t n)
{
    uint16_t crc = 0xFFFF;
    if (!p)
        return 0;
    while (n--)
    {
        crc ^= (uint16_t)*p++ << 8;
        for (unsigned b = 0; b < 8; b++)
            crc = (uint16_t)((crc & 0x8000U) ? ((uint32_t)crc << 1) ^ 0x1021U : (uint32_t)crc << 1);
    }
    return crc;
}

/**
 * @brief 打包 9 字节 ACK 应答帧
 * @param[in]  seq   被确认遥测帧的序号
 * @param[in]  token 遥测帧对应的 CRC-16 令牌
 * @param[out] o     输出缓冲区指针
 * @param[in]  cap   输出缓冲区容量 (必须 >= 9)
 * @return 实际写入的帧长度 (9)
 */
uint16_t srv_protocol_pack_ack(uint8_t seq, uint16_t token, uint8_t *o, uint16_t cap)
{
    if (!o || cap < 9)
        return 0;
    /* 写入帧头与元数据: 0xAA 0x55, seq, len=2, cmd=0x80 */
    frame_header(o, seq, 2, PROTOCOL_CMD_ACK);
    /* 写入 16 位令牌 (小端序) */
    put16(o + 5, token);
    /* 计算校验和: 对 [2]~[6] 共 5 字节求累加和 */
    o[7] = srv_protocol_calc_checksum(o + 2, 5);
    /* 写入帧尾: 0x0D */
    o[8] = 0x0D;
    return 9;
}

/**
 * @brief 初始化流式协议解析器结构体
 * @param[out] p 解析器结构体指针
 */
void protocol_parser_init(protocol_parser_t *p)
{
    if (p)
        memset(p, 0, sizeof(*p));
}

/**
 * @brief 内部辅助函数: 丢弃当前缓存的第一个字节并将后续字节前移
 * @param[in,out] p 解析器结构体指针
 */
static void discard_first(protocol_parser_t *p)
{
    if (p->used)
    {
        --p->used;
        memmove(p->bytes, p->bytes + 1, p->used);
    }
}

/**
 * @brief 向协议解析器喂入单字节并执行滑动容错解析
 * 
 * 核心特性:
 * 1. 具备帧间停顿超时复位机制 (超过 PARSER_GAP_MS 则清空残余帧)；
 * 2. 具备前缀滑动查找机制: 即使接收流包含乱码、杂音或截断碎片，
 *    只要出现合法的 0xAA 0x55 ... 完整帧，均能通过单字节逐一滑动正确锁定并解出。
 * 
 * @param[in,out] p    解析器状态结构体指针
 * @param[in]     byte 新接收的字节
 * @param[in]     now  当前系统毫秒时间戳
 * @param[out]    o    输出成功解包的协议帧结构体
 * @return true 成功解析出一帧完整数据; false 尚在接收中或已滤除杂音
 */
bool protocol_parser_feed(protocol_parser_t *p, uint8_t byte, uint32_t now, protocol_packet_t *o)
{
    if (!p || !o)
        return false;
    /* 检查字节间接收超时 (防止半截损坏帧长期占用解析缓存) */
    if (p->used && (uint32_t)(now - p->last_byte_ms) > PARSER_GAP_MS)
    {
        p->used = 0;
        p->errors++;
    }
    p->last_byte_ms = now;
    /* 若缓冲区已满仍无有效帧，丢弃首字节腾出空间 */
    if (p->used >= sizeof(p->bytes))
    {
        discard_first(p);
        p->errors++;
    }
    /* 将新字节追加到尾部 */
    p->bytes[p->used++] = byte;
    /* 循环滑动匹配有效帧: 遇到非法字节仅丢弃首字节继续尝试后续内容 */
    while (p->used)
    {
        /* 校验第1个帧头字节是否为 0xAA */
        if (p->bytes[0] != 0xAA)
        {
            discard_first(p);
            continue;
        }
        /* 字节数不足以校验第2个帧头，等待更多字节 */
        if (p->used < 2)
            return false;
        /* 校验第2个帧头字节是否为 0x55 */
        if (p->bytes[1] != 0x55)
        {
            discard_first(p);
            continue;
        }
        /* 字节数不足以读取序列号与长度字段，等待更多字节 */
        if (p->used < 4)
            return false;
        uint8_t len = p->bytes[3];
        /* 载荷长度只能为 16 (遥测) 或 2 (ACK) */
        if (len != 16 && len != 2)
        {
            p->errors++;
            discard_first(p);
            continue;
        }
        /* 字节数不足以读取功能码，等待更多字节 */
        if (p->used < 5)
            return false;
        /* 校验功能码与长度是否一致 */
        if (!((len == 16 && p->bytes[4] == PROTOCOL_CMD_TELEMETRY) ||
              (len == 2 && p->bytes[4] == PROTOCOL_CMD_ACK)))
        {
            p->errors++;
            discard_first(p);
            continue;
        }
        /* 计算当前帧需要的总长度: 2(头) + 1(seq) + 1(len) + 1(cmd) + len + 1(chk) + 1(尾) = len + 7 */
        uint8_t total = (uint8_t)(len + 7);
        if (p->used < total)
            return false;
        /* 校验帧尾 0x0D 与累加和校验码 */
        if (p->bytes[total - 1] != 0x0D ||
            p->bytes[total - 2] != srv_protocol_calc_checksum(p->bytes + 2, (uint16_t)(len + 3)))
        {
            p->errors++;
            discard_first(p);
            continue;
        }
        /* 校验通过，提取数据包 */
        o->seq = p->bytes[2];
        o->cmd = p->bytes[4];
        o->len = len;
        memcpy(o->payload, p->bytes + 5, len);
        /* 移除已解析完成的整帧数据 */
        p->used = (uint8_t)(p->used - total);
        memmove(p->bytes, p->bytes + total, p->used);
        return true;
    }
    return false;
}
