#ifndef SRV_PROTOCOL_H
#define SRV_PROTOCOL_H

/**
 * @file srv_protocol.h
 * @brief 自定义无线串口通信协议与帧打包/解包/校验服务
 * 
 * 帧格式规范:
 * 1. 遥测数据帧 (总长 23 字节, CMD=0x01):
 *    [0..1]:  帧头 0xAA 0x55 (2B)
 *    [2]:     包序号 SEQ (1B, 0~255 循环递增)
 *    [3]:     有效载荷长度 LEN = 16 (0x10, 1B)
 *    [4]:     功能码 CMD = 0x01 (1B)
 *    [5..20]: 遥测载荷 PAYLOAD (16B, 小端格式)
 *             - joy_x_raw, joy_y_raw (4B, int16_t, 归一化 -1000~+1000)
 *             - joy_x_mv, joy_y_mv   (4B, uint16_t, 物理电压 0~3300mV)
 *             - key_mask             (1B, 低4位为按键K1~K4, 高4位为系统故障/标定状态)
 *             - switch_mask          (1B, 拨码开关SW1~SW2)
 *             - pitch_cd, roll_cd, yaw_cd (6B, int16_t, 0.1度单位)
 *    [21]:    累加和校验 CHECKSUM (1B, 对从SEQ到PAYLOAD末尾共19字节求和后取低8位)
 *    [22]:    帧尾 0x0D ('\r', 1B)
 * 
 * 2. 应答帧 (总长 9 字节, CMD=0x80):
 *    [0..1]:  帧头 0xAA 0x55 (2B)
 *    [2]:     对应遥测帧的包序号 SEQ (1B)
 *    [3]:     有效载荷长度 LEN = 2 (1B)
 *    [4]:     功能码 CMD = 0x80 (1B)
 *    [5..6]:  CRC-16/CCITT 令牌 Token (2B, 小端格式, 对应原遥测全帧的CRC校验码)
 *    [7]:     累加和校验 CHECKSUM (1B, 对[2]~[6]共5字节求和)
 *    [8]:     帧尾 0x0D (1B)
 */

#include <stdint.h>
#include <stdbool.h>

/** @brief 帧头第一字节: 0xAA */
#define PROTOCOL_FRAME_HEADER_1 0xAAU
/** @brief 帧头第二字节: 0x55 */
#define PROTOCOL_FRAME_HEADER_2 0x55U
/** @brief 帧尾字节: 0x0D ('\r') */
#define PROTOCOL_FRAME_TAIL 0x0DU

/** @brief 遥测上报功能码 */
#define PROTOCOL_CMD_TELEMETRY 0x01U
/** @brief 遥测应答功能码 (ACK) */
#define PROTOCOL_CMD_ACK 0x80U

/** @brief 遥测载荷字节长度 (16 字节) */
#define PROTOCOL_PAYLOAD_LEN 16U
/** @brief 遥测完整帧字节长度 (23 字节) */
#define PROTOCOL_FRAME_TOTAL_LEN 23U
/** @brief ACK 应答完整帧字节长度 (9 字节) */
#define PROTOCOL_ACK_LEN 9U

/* key_mask 状态位掩码定义: 低 4 位 (bit0~bit3) 代表按键 K1~K4, 高 4 位代表外设健康/标定状态 */
/** @brief bit4: IMU 故障标志 (未连接或通信超时) */
#define TELEMETRY_IMU_FAULT 0x10U
/** @brief bit5: IMU 正在静态标定中 */
#define TELEMETRY_CALIBRATING 0x20U
/** @brief bit6: 摇杆 ADC 故障标志 (DMA 采样未就绪或未初始化) */
#define TELEMETRY_ADC_FAULT 0x40U
/** @brief bit7: OLED 显示屏故障标志 (I2C 挂死或应答失败) */
#define TELEMETRY_OLED_FAULT 0x80U

#pragma pack(push, 1)
/**
 * @brief 遥测核心数据载荷结构体 (16 字节, 紧凑单字节对齐)
 */
typedef struct
{
    int16_t joy_x_raw, joy_y_raw;       /* 摇杆归一化输出: -1000 ~ +1000 (0为中心死区) */
    uint16_t joy_x_mv, joy_y_mv;        /* 摇杆采样电压: 0 ~ 3300 mV */
    uint8_t key_mask, switch_mask;      /* key_mask: 低4位按键状态,高4位故障; switch_mask: 低2位拨码开关状态 */
    int16_t pitch_cd, roll_cd, yaw_cd; /* 欧拉角姿态 (0.1度单位, 例如 1800 对应 180.0°) */
} telemetry_payload_t;
#pragma pack(pop)

/** @brief 静态断言: 编译期严格确保 telemetry_payload_t 占 16 字节无填充 */
typedef char telemetry_size_check[(sizeof(telemetry_payload_t) == 16) ? 1 : -1];

/**
 * @brief 流式协议解析器上下文结构体
 */
typedef struct
{
    uint8_t bytes[23], used;     /* 滑动解析缓存区及当前已有字节数 */
    uint32_t errors, last_byte_ms; /* 错误计数与最近接收字节时刻 (毫秒) */
} protocol_parser_t;

/**
 * @brief 解析出的解包数据包结构体
 */
typedef struct
{
    uint8_t seq, cmd, len, payload[16]; /* 包序号、功能码、载荷长度与载荷内容 */
} protocol_packet_t;

/**
 * @brief 协议层初始化函数 (占位，保持接口完整)
 */
void srv_protocol_init(void);

/**
 * @brief 计算指定数据块的单字节累加和 (Checksum)
 * @param[in] data 待校验数据指针
 * @param[in] len  数据字节长度
 * @return 8位累加和校验值
 */
uint8_t srv_protocol_calc_checksum(const uint8_t *data, uint16_t len);

/**
 * @brief 打包遥测数据为 23 字节的完整无线发送帧
 * @param[in]  seq 包序号 (0~255)
 * @param[in]  p   指向待打包的遥测结构体指针
 * @param[out] out 存放生成帧字节的输出缓冲区
 * @param[in]  cap 输出缓冲区容量 (必须 >= 23)
 * @return 成功打包返回写入的字节数 (23)，失败返回 0
 */
uint16_t srv_protocol_pack(uint8_t seq, const telemetry_payload_t *p, uint8_t *out, uint16_t cap);

/**
 * @brief 从解包出的协议包中解析并恢复遥测数据载荷结构体
 * @param[in]  packet 指向已校验通过的数据包指针
 * @param[out] out    输出存放解析出遥测数据的结构体指针
 * @return true 成功解析; false 功能码或载荷长度不匹配
 */
bool srv_protocol_unpack(const protocol_packet_t *packet, telemetry_payload_t *out);

/**
 * @brief 对完整 23 字节数据帧计算 CRC-16/CCITT 令牌 (Token)
 * @param[in] data 数据帧指针
 * @param[in] len  帧长度 (23)
 * @return 16位 CRC 校验令牌，用于在 ACK 中唯一标识并确认此数据帧
 */
uint16_t srv_protocol_token(const uint8_t *data, uint16_t len);

/**
 * @brief 打包 9 字节的 ACK 应答帧
 * @param[in]  seq   被确认遥测帧的序号
 * @param[in]  token 对应的 16位 CRC 令牌
 * @param[out] out   输出缓冲区
 * @param[in]  cap   缓冲区容量 (需 >= 9)
 * @return 成功返回打包字节数 (9)，失败返回 0
 */
uint16_t srv_protocol_pack_ack(uint8_t seq, uint16_t token, uint8_t *out, uint16_t cap);

/**
 * @brief 初始化流式协议解析器
 * @param[out] p 解析器结构体指针
 */
void protocol_parser_init(protocol_parser_t *p);

/**
 * @brief 流式喂入单字节并执行容错滑动解析
 * @param[in,out] p    解析器状态结构体指针
 * @param[in]     byte 新接收到的单字节
 * @param[in]     now  当前系统时间戳 (毫秒), 用于帧间超时判定
 * @param[out]    out  若检测并验证出完整有效帧，则填充至此结构体
 * @return true 解析出完整有效帧; false 帧未完成或校验失败
 * @note 必须在主循环中调用，不可在 ISR 中调用。具备错误前缀滑动恢复功能。
 */
bool protocol_parser_feed(protocol_parser_t *p, uint8_t byte, uint32_t now, protocol_packet_t *out);

#endif
