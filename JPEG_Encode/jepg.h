// jpegzc.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <stdio.h>
#include "malloc.h"

typedef unsigned int UINT32;
typedef int    INT32;
typedef unsigned short UINT16;
typedef short   INT16;
typedef unsigned char UINT8;
//typedef char   INT8;

//clamp color to [0,0xFF]
#define CLIP(color) (unsigned char)(((color) > 0xFF) ? 0xFF : (((color) < 0) ? 0 : (color)))
#define  BLOCK_SIZE 64  

//JPEG编码器
typedef struct JPEG_ENCODER_STRUCTURE
{
	UINT16 mcu_width;
	UINT16 mcu_height;
	UINT16 horizontal_mcus;
	UINT16 vertical_mcus;
	UINT16 rows_in_bottom_mcus;
	UINT16 cols_in_right_mcus;
	UINT16 length_minus_mcu_width;
	UINT16 length_minus_width;
	UINT16 mcu_width_size;
	UINT16 offset;
	INT16 ldc1;
	INT16 ldc2;
	INT16 ldc3;
	UINT16 rows;
	UINT16 cols;
	UINT16 incr;
}JPEG_ENCODER_STRUCTURE;


UINT8  Lqt[BLOCK_SIZE];//亮度 量化表
UINT8  Cqt[BLOCK_SIZE];//色度 量化表
UINT16  ILqt[BLOCK_SIZE];//经DSP除法后的Lqt，结合一定操作实现向上取整除法
UINT16  ICqt[BLOCK_SIZE];//经DSP除法后的Cqt，结合一定操作实现向上取整除法
INT16  Y1[BLOCK_SIZE];//YCbcr422 MCU Y1
INT16  Y2[BLOCK_SIZE];//YCbcr422 MCU Y1
INT16  Y3[BLOCK_SIZE];//YCbcr422中未使用
INT16  Y4[BLOCK_SIZE];//YCbcr422中未使用
INT16  CB[BLOCK_SIZE];//YCbcr422 MCU CB
INT16  CR[BLOCK_SIZE];//YCbcr422 MCU CR
INT16  Temp[BLOCK_SIZE];//将量化后数据转为ZigZag排列存储在Temp中
INT32  lcode;//待序列化数据
UINT16  bitindex;//当前待序列化数据在一个字（4B）中的bit位

//zigzag 排序表
static UINT8 zigzag_table[] = {
	0,	1,	5,	6,	14, 15, 27, 28,
	2,	4,	7,	13,	16, 26, 29, 42,
	3,	8,	12,	17,	25, 30, 41, 43, 
	9,	11, 18, 24, 31, 40, 44, 53,
	10, 19, 23, 32, 39, 45, 52, 54, 
	20, 22, 33, 38, 46, 51, 55, 60, 
	21, 34, 37, 47, 50, 56, 59, 61,
	35, 36, 48, 49, 57, 58, 62, 63 
};


// Header for JPEG Encoder JPEG编码器头部标志信息
static UINT16 markerdata[] = { 
    // dht
    0xFFC4, 0x1A2, 0x00, 
    // luminance dc (2 - 16) + 1
    0x0105, 0x0101, 0x00101, 0x0101, 0x0000, 0x00000, 00000, 00000, 
    // luminance dc (2 - 12) + 1
    0x0102, 0x0304, 0x0506, 0x0708, 0x090A, 0x0B01, 
    // chrominance dc (1 - 16)
    0x0003, 0x0101, 0x0101, 0x0101, 0x0101, 0x0100, 0x0000, 0x0000, 
    // chrominance dc (1 - 12)
    0x0001, 0x00203, 0x0405, 0x0607, 0x0809, 0x00A0B, 
    // luminance ac 1 + (1 - 15)
    0x1000, 0x0201, 0x0303, 0x0204, 0x0305, 0x0504, 0x0400, 0x0001, 
    // luminance ac 1 + (1 - 162) + 1
    0x7D01, 0x0203, 0x0004, 0x1105, 0x1221, 0x3141, 0x0613, 0x5161, 0x0722,
    0x7114, 0x3281, 0x91A1, 0x0823, 0x42B1, 0xC115, 0x52D1, 0xF024, 0x3362,
    0x7282, 0x090A, 0x1617, 0x1819, 0x1A25, 0x2627, 0x2829, 0x2A34, 0x3536,
    0x3738, 0x393A, 0x4344, 0x4546, 0x4748, 0x494A, 0x5354, 0x5556, 0x5758,
    0x595A, 0x6364, 0x6566, 0x6768, 0x696A, 0x7374, 0x7576, 0x7778, 0x797A,
    0x8384, 0x8586, 0x8788, 0x898A, 0x9293, 0x9495, 0x9697, 0x9899, 0x9AA2,
    0xA3A4, 0xA5A6, 0xA7A8, 0xA9AA, 0xB2B3, 0xB4B5, 0xB6B7, 0xB8B9, 0xBAC2,
    0xC3C4, 0xC5C6, 0xC7C8, 0xC9CA, 0xD2D3, 0xD4D5, 0xD6D7, 0xD8D9, 0xDAE1,
    0xE2E3, 0xE4E5, 0xE6E7, 0xE8E9, 0xEAF1, 0xF2F3, 0xF4F5, 0xF6F7, 0xF8F9,
    0xFA11, 
    // chrominance ac (1 - 16)
    0x0002, 0x0102, 0x0404, 0x0304, 0x0705, 0x0404, 0x0001, 0x0277, 
    // chrominance ac (1 - 162)
 0x0001, 0x0203, 0x1104, 0x0521, 0x3106, 0x1241, 0x5107, 0x6171, 0x1322,
    0x3281, 0x0814, 0x4291, 0xA1B1, 0xC109, 0x2333, 0x52F0, 0x1562, 0x72D1,
    0x0A16, 0x2434, 0xE125, 0xF117, 0x1819, 0x1A26, 0x2728, 0x292A, 0x3536,
    0x3738, 0x393A, 0x4344, 0x4546, 0x4748, 0x494A, 0x5354, 0x5556, 0x5758,
    0x595A, 0x6364, 0x6566, 0x6768, 0x696A, 0x7374, 0x7576, 0x7778, 0x797A,
    0x8283, 0x8485, 0x8687, 0x8889, 0x8A92, 0x9394, 0x9596, 0x9798, 0x999A,
    0xA2A3, 0xA4A5, 0xA6A7, 0xA8A9, 0xAAB2, 0xB3B4, 0xB5B6, 0xB7B8, 0xB9BA,
    0xC2C3, 0xC4C5, 0xC6C7, 0xC8C9, 0xCAD2, 0xD3D4, 0xD5D6, 0xD7D8, 0xD9DA,
    0xE2E3, 0xE4E5, 0xE6E7, 0xE8E9, 0xEAF2, 0xF3F4, 0xF5F6, 0xF7F8, 0xF9FA 
};

//0-255 所需的bit数
UINT8 bitsize[] = {
    0, 1, 2, 2, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4, 
	5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 
	6, 6, 6, 6, 6, 6, 6, 6, 
	6, 6, 6, 6, 6, 6, 6, 6, 
	6, 6, 6, 6, 6, 6, 6, 6, 
	6, 6, 6, 6, 6, 6, 6, 6, 
	7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 7, 7, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8
};

//量度 DC系数Huffman Code表
UINT16 luminance_dc_code_table[] = {
 0x0000, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x000E, 0x001E, 0x003E,
    0x007E, 0x00FE, 0x01FE
};

//量度 DC系数Huffman Code Size表
UINT16 luminance_dc_size_table[] = {
 0x0002, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0004, 0x0005, 0x0006,
 0x0007, 0x0008, 0x0009
};

//色度 DC系数Huffman Code表
UINT16 chrominance_dc_code_table[] = {
 0x0000, 0x0001, 0x0002, 0x0006, 0x000E, 0x001E, 0x003E, 0x007E, 0x00FE,
 0x01FE, 0x03FE, 0x07FE
};

//色度 DC系数Huffman Code Size表
UINT16 chrominance_dc_size_table[] = {
 0x0002, 0x0002, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008,
 0x0009, 0x000A, 0x000B 
};

//亮度 AC系数 Huffman Code表
UINT16 luminance_ac_code_table[] = {
    0x000A, 0x0000, 0x0001, 0x0004, 0x000B, 0x001A, 0x0078, 0x00F8, 0x03F6, 0xFF82,
	0xFF83, 0x000C, 0x001B, 0x0079, 0x01F6, 0x07F6, 0xFF84, 0xFF85, 0xFF86, 0xFF87,
	0xFF88, 0x001C, 0x00F9, 0x03F7, 0x0FF4, 0xFF89, 0xFF8A, 0xFF8b, 0xFF8C, 0xFF8D,
	0xFF8E, 0x003A, 0x01F7, 0x0FF5, 0xFF8F, 0xFF90, 0xFF91, 0xFF92, 0xFF93, 0xFF94,
	0xFF95, 0x003B, 0x03F8, 0xFF96, 0xFF97, 0xFF98, 0xFF99, 0xFF9A, 0xFF9B, 0xFF9C,
	0xFF9D, 0x007A, 0x07F7, 0xFF9E, 0xFF9F, 0xFFA0, 0xFFA1, 0xFFA2, 0xFFA3, 0xFFA4,
	0xFFA5, 0x007B, 0x0FF6, 0xFFA6, 0xFFA7, 0xFFA8, 0xFFA9, 0xFFAA, 0xFFAB, 0xFFAC, 
	0xFFAD, 0x00FA, 0x0FF7, 0xFFAE, 0xFFAF, 0xFFB0, 0xFFB1, 0xFFB2, 0xFFB3, 0xFFB4,
	0xFFB5, 0x01F8, 0x7FC0, 0xFFB6, 0xFFB7, 0xFFB8, 0xFFB9, 0xFFBA, 0xFFBB, 0xFFBC,
    0xFFBD, 0x01F9, 0xFFBE, 0xFFBF, 0xFFC0, 0xFFC1, 0xFFC2, 0xFFC3, 0xFFC4, 0xFFC5, 
	0xFFC6, 0x01FA, 0xFFC7, 0xFFC8, 0xFFC9, 0xFFCA, 0xFFCB, 0xFFCC, 0xFFCD, 0xFFCE, 
	0xFFCF, 0x03F9, 0xFFD0, 0xFFD1, 0xFFD2, 0xFFD3, 0xFFD4, 0xFFD5, 0xFFD6, 0xFFD7, 
	0xFFD8, 0x03FA, 0xFFD9, 0xFFDA, 0xFFDB, 0xFFDC, 0xFFDD, 0xFFDE, 0xFFDF, 0xFFE0, 
	0xFFE1, 0x07F8, 0xFFE2, 0xFFE3, 0xFFE4, 0xFFE5, 0xFFE6, 0xFFE7, 0xFFE8, 0xFFE9, 
	0xFFEA, 0xFFEB, 0xFFEC, 0xFFED, 0xFFEE, 0xFFEF, 0xFFF0, 0xFFF1, 0xFFF2, 0xFFF3, 
	0xFFF4, 0xFFF5, 0xFFF6, 0xFFF7, 0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 
	0xFFFE, 0x07F9
};

//亮度 AC系数Huffman Size表
UINT16 luminance_ac_size_table[] = {
    0x0004, 0x0002, 0x0002, 0x0003, 0x0004, 0x0005, 0x0007, 0x0008, 0x000A,
    0x0010, 0x0010, 0x0004, 0x0005, 0x0007, 0x0009, 0x000B, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0005, 0x0008, 0x000A, 0x000C, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0006, 0x0009, 0x000C, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0006, 0x000A, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0007, 0x000B, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0007, 0x000C,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0008,
    0x000C, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0009, 0x000F, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x000A, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x000A, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x000B, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x000B
};

//色度AC系数Huffman表
UINT16 chrominance_ac_code_table[] = {
    0x0000, 0x0001, 0x0004, 0x000A, 0x0018, 0x0019, 0x0038, 0x0078, 0x01F4,
    0x03F6, 0x0FF4, 0x000B, 0x0039, 0x00F6, 0x01F5, 0x07F6, 0x0FF5, 0xFF88,
    0xFF89, 0xFF8A, 0xFF8B, 0x001A, 0x00F7, 0x03F7, 0x0FF6, 0x7FC2, 0xFF8C,
    0xFF8D, 0xFF8E, 0xFF8F, 0xFF90, 0x001B, 0x00F8, 0x03F8, 0x0FF7, 0xFF91,
    0xFF92, 0xFF93, 0xFF94, 0xFF95, 0xFF96, 0x003A, 0x01F6, 0xFF97, 0xFF98,
    0xFF99, 0xFF9A, 0xFF9B, 0xFF9C, 0xFF9D, 0xFF9E, 0x003B, 0x03F9, 0xFF9F,
    0xFFA0, 0xFFA1, 0xFFA2, 0xFFA3, 0xFFA4, 0xFFA5, 0xFFA6, 0x0079, 0x07F7,
    0xFFA7, 0xFFA8, 0xFFA9, 0xFFAA, 0xFFAB, 0xFFAC, 0xFFAD, 0xFFAE, 0x007A,
    0x07F8, 0xFFAF, 0xFFB0, 0xFFB1, 0xFFB2, 0xFFB3, 0xFFB4, 0xFFB5, 0xFFB6,
    0x00F9, 0xFFB7, 0xFFB8, 0xFFB9, 0xFFBA, 0xFFBB, 0xFFBC, 0xFFBD, 0xFFBE,
    0xFFBF, 0x01F7, 0xFFC0, 0xFFC1, 0xFFC2, 0xFFC3, 0xFFC4, 0xFFC5, 0xFFC6,
    0xFFC7, 0xFFC8, 0x01F8, 0xFFC9, 0xFFCA, 0xFFCB, 0xFFCC, 0xFFCD, 0xFFCE,
    0xFFCF, 0xFFD0, 0xFFD1, 0x01F9, 0xFFD2, 0xFFD3, 0xFFD4, 0xFFD5, 0xFFD6,
    0xFFD7, 0xFFD8, 0xFFD9, 0xFFDA, 0x01FA, 0xFFDB, 0xFFDC, 0xFFDD, 0xFFDE,
    0xFFDF, 0xFFE0, 0xFFE1, 0xFFE2, 0xFFE3, 0x07F9, 0xFFE4, 0xFFE5, 0xFFE6,
    0xFFE7, 0xFFE8, 0xFFE9, 0xFFEA, 0xFFEb, 0xFFEC, 0x3FE0, 0xFFED, 0xFFEE,
    0xFFEF, 0xFFF0, 0xFFF1, 0xFFF2, 0xFFF3, 0xFFF4, 0xFFF5, 0x7FC3, 0xFFF6,
    0xFFF7, 0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE,
    0x03FA
};

//色度AC系数Huffman Size表
UINT16 chrominance_ac_size_table[] = {
    0x0002, 0x0002, 0x0003, 0x0004, 0x0005, 0x0005, 0x0006, 0x0007, 0x0009,
    0x000A, 0x000C, 0x0004, 0x0006, 0x0008, 0x0009, 0x000B, 0x000C, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0005, 0x0008, 0x000A, 0x000C, 0x000F, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0005, 0x0008, 0x000A, 0x000C, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0006, 0x0009, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0006, 0x000A, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0007, 0x000B,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0007,
    0x000B, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0008, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x000B, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x000E, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x000F, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x000A 
};


#define  FOUR_ZERO_ZERO 0 //Grey scale Y00 ... YCbCr400
#define  FOUR_TWO_TWO 2 //Y00 Cb Y01 Cr YCbCr422

//指定读取MCU数据的函数
static void (*read_format) (JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,UINT8 * input_ptr);

/*
色彩空间转换
将RGB图像数据转为YCbCr400（未使用），本算法默认采用RGB_2_422，该方法未使用

|input_ptr		|	输入图像指针															|
|output_ptr		|	输出图像指针（无效），采用原地转换方式，直接在修改输入指针指向的数据	|
|image_width	|	图像width																|
|image_height	|	图像height																|
*/
static void RGB_2_400 (UINT8 * input_ptr, UINT8 * output_ptr, UINT32 image_width,UINT32 image_height) 
{
	UINT32 i, size;
	UINT8 R, G, B;
	INT32 Y;
	UINT8 * inbuf = input_ptr;
	size = image_width * image_height;
  
	for (i = size; i > 0; i--)
    {
		B = inbuf[0];
		G = inbuf[1];
		R = inbuf[2];
		inbuf += 3;
		Y = CLIP((77 * R + 150 * G + 29 * B) >> 8);
		*input_ptr++ = (UINT8) Y;
    }
}

/*
按照YCbCr400格式读取一个MCU数据（未使用），本算法采用YCbCr422格式，该方法未使用

|jpeg_encoder_structure		|	JPEG编码器		|
|input_ptr					|	输入图像指针	|
*/
static void read_400_format (JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,UINT8 * input_ptr) 
{
	INT32 i, j;
	INT16 * Y1_Ptr = Y1;
	UINT16 rows = jpeg_encoder_structure->rows;
	UINT16 cols = jpeg_encoder_structure->cols;
	UINT16 incr = jpeg_encoder_structure->incr;
	
	for (i = rows; i > 0; i--)
	{
		for (j = cols; j > 0; j--)
		{
			*Y1_Ptr++ = *input_ptr++ - 128;
		}

		for (j = 8 - cols; j > 0; j--)
		{
			*Y1_Ptr++ = *(Y1_Ptr - 1);
		}
		input_ptr += incr;
	}
 
	for (i = 8 - rows; i > 0; i--)
	{
		for (j = 8; j > 0; j--)
		{
			*Y1_Ptr++ = *(Y1_Ptr - 8);
		}
	}
}


/*
将RGB格式转为YUV422格式

|input_ptr		|	输入图像															|
|output_ptr		|	输出图像（无效），本算法采取原地输出，直接修改输入指针指向的数据	|
|image_width	|	图像width															|
|image_height	|	图像height															|
*/
static void RGB_2_422 (UINT8 * input_ptr, UINT8 * output_ptr, UINT32 image_width, UINT32 image_height) 
{
	UINT32 i, size;
	UINT8 R, G, B, R1, G1, B1;
	INT32 Y, Yp, Cb, Cr;
	UINT8 * inbuf = input_ptr;
	size = image_width * image_height / 2;
	
	for (i = size; i > 0; i--)
	{
		  B = inbuf[0];
		  G = inbuf[1];
		  R = inbuf[2];
		  B1 = inbuf[3];
		  G1 = inbuf[4];
		  R1 = inbuf[5];
		  inbuf += 6;
		  Y = CLIP ((77 * R + 150 * G + 29 * B) >> 8);
		  Yp = CLIP ((77 * R1 + 150 * G1 + 29 * B1) >> 8);
		  Cb = CLIP (((-43 * R - 85 * G + 128 * B) >> 8) + 128);
		  Cr = CLIP (((128 * R - 107 * G - 21 * B) >> 8) + 128);
		  *input_ptr++ = (UINT8) Y;
		  *input_ptr++ = (UINT8) Cb;
		  *input_ptr++ = (UINT8) Yp;
		  *input_ptr++ = (UINT8) Cr;
	}
}


/*This function implements 16 Step division for Q.15 format data 
移位相减除法 
后面结合
	value = data[i] * quant_table_ptr[i];
	value = (value + 0x4000) >> 15;
实现除法，等效于向上取整除法
*/ 
UINT16 DSP_Division (UINT32 numer, UINT32 denom) 
{
	 UINT16 i;
	 denom <<= 15;
	 for (i = 16; i > 0; i--)
	 {
		if (numer > denom)
		{
			numer -= denom;
			numer <<= 1;
			numer++;
		}
		else 
		{
			numer <<= 1;
		}
	 }
	 return (UINT16) numer;
}

/*
初始化，写入JPEG头部标志信息

|output_ptr		|	输出图像指针														|
|image_format	|	图像格式（无效），根据格式写入不同的标志信息，本算法默认YCbCr422	|
|image_width	|	图像width															|
|image_height	|	图像height															|
*/
UINT8 * write_markers(UINT8 * output_ptr, UINT32 image_format,UINT32 image_width, UINT32 image_height) 
{
	UINT16 i, header_length;
	UINT8 number_of_components;
 
	// Start of image marker
	*output_ptr++ = 0xFF;
	*output_ptr++ = 0xD8;
 
	// Quantization table marker
	*output_ptr++ = 0xFF;
	*output_ptr++ = 0xDB;
 
	// Quantization table length
	*output_ptr++ = 0x00;
	*output_ptr++ = 0x84;
 
	// Pq, Tq
	*output_ptr++ = 0x00;
 
	// Lqt table
	for (i = 0; i < 64; i++) 
	{
		*output_ptr++ = Lqt[i];
	}

	// Pq, Tq
	*output_ptr++ = 0x01;
 
	// Cqt table
	for (i = 0; i < 64; i++)
	{
		*output_ptr++ = Cqt[i];
	}

	// huffman table(DHT)
	for (i = 0; i < 210; i++)
	{
		*output_ptr++ = (UINT8) (markerdata[i] >> 8);
		*output_ptr++ = (UINT8) markerdata[i];
	}

	//FOUR_TWO_TWO	
	number_of_components = 3;
 
	// Frame header(SOF)
	// Start of frame marker
	*output_ptr++ = 0xFF;
	*output_ptr++ = 0xC0;
	header_length = (UINT16) (8 + 3 * number_of_components);
 
	// Frame header length  
	*output_ptr++ = (UINT8) (header_length >> 8);
	*output_ptr++ = (UINT8) header_length;
 
	// Precision (P)
	*output_ptr++ = 0x08;
 
	// image height
	*output_ptr++ = (UINT8) (image_height >> 8);
	*output_ptr++ = (UINT8) image_height;
 
	// image width
	*output_ptr++ = (UINT8) (image_width >> 8);
	*output_ptr++ = (UINT8) image_width;
 
	// Nf
	*output_ptr++ = number_of_components;
 
	// FOUR_TWO_TWO
    *output_ptr++ = 0x01;
    *output_ptr++ = 0x21;
    *output_ptr++ = 0x00;
    *output_ptr++ = 0x02;
    *output_ptr++ = 0x11;
    *output_ptr++ = 0x01;
    *output_ptr++ = 0x03;
    *output_ptr++ = 0x11;
    *output_ptr++ = 0x01;
 
	// Scan header(SOF)
	// Start of scan marker
	*output_ptr++ = 0xFF;
	*output_ptr++ = 0xDA;
	header_length = (UINT16) (6 + (number_of_components << 1));
	
	// Scan header length
	*output_ptr++ = (UINT8) (header_length >> 8);
	*output_ptr++ = (UINT8) header_length;
	// Ns
	*output_ptr++ = number_of_components;
	
	//FOUR_TWO_TWO
	*output_ptr++ = 0x01;
    *output_ptr++ = 0x00;
    *output_ptr++ = 0x02;
    *output_ptr++ = 0x11;
    *output_ptr++ = 0x03;
    *output_ptr++ = 0x11;
	*output_ptr++ = 0x00;
	*output_ptr++ = 0x3F;
	*output_ptr++ = 0x00;
 return output_ptr;
}

/*
读取YCbCr422格式的一个MCU数据

|jpeg_encoder_structure |	JPEG编码器		|
|input_ptr				|	输入图像指针	|
*/
static void read_422_format (JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT8 * input_ptr) 
{
	INT32 i, j;
	UINT16 Y1_cols, Y2_cols;
	INT16 * Y1_Ptr = Y1;
	INT16 * Y2_Ptr = Y2;
	INT16 * CB_Ptr = CB;
	INT16 * CR_Ptr = CR;
	UINT16 rows = jpeg_encoder_structure->rows;
	UINT16 cols = jpeg_encoder_structure->cols;
	UINT16 incr = jpeg_encoder_structure->incr;
 
	if (cols <= 8)
	{
		Y1_cols = cols;
		Y2_cols = 0;
    }
	else
    {
		Y1_cols = 8;
		Y2_cols = (UINT16) (cols - 8);
    }
 
	for (i = rows; i > 0; i--)
    {
		for (j = Y1_cols >> 1; j > 0; j--)
		{
		   *Y1_Ptr++ = *input_ptr++ - 128;
		   *CB_Ptr++ = *input_ptr++ - 128;
		   *Y1_Ptr++ = *input_ptr++ - 128;
		   *CR_Ptr++ = *input_ptr++ - 128;
		}
		
		for (j = Y2_cols >> 1; j > 0; j--) 
		{
		   *Y2_Ptr++ = *input_ptr++ - 128;
		   *CB_Ptr++ = *input_ptr++ - 128;
		   *Y2_Ptr++ = *input_ptr++ - 128;
		   *CR_Ptr++ = *input_ptr++ - 128;
		}
  
		//列数不足8
		if (cols <= 8)  
		{
			for (j = 8 - Y1_cols; j > 0; j--) 
			{
				*Y1_Ptr++ = *(Y1_Ptr - 1);
			}
			for (j = 8 - Y2_cols; j > 0; j--)
			{
				*Y2_Ptr++ = *(Y1_Ptr - 1);
			}
		}
		else
		{
			//Y2列数不足8
			for (j = 8 - Y2_cols; j > 0; j--)
			{
				*Y2_Ptr++ = *(Y2_Ptr - 1);
			}
		}

		//Cb、Cr列数不足8 补全
		for (j = (16 - cols) >> 1; j > 0; j--)
		{
		   *CB_Ptr++ = *(CB_Ptr - 1);
		   *CR_Ptr++ = *(CR_Ptr - 1);
		}
		input_ptr += incr;
    }
 
	//行数不足8 补齐剩余的
	for (i = 8 - rows; i > 0; i--)
    {
		for (j = 8; j > 0; j--) 
		{
		   *Y1_Ptr++ = *(Y1_Ptr - 8);
		   *Y2_Ptr++ = *(Y2_Ptr - 8);
		   *CB_Ptr++ = *(CB_Ptr - 8);
		   *CR_Ptr++ = *(CR_Ptr - 8);
		}
    }
}

/*
初始化JPEG Encoder
*/
static void initialization (JPEG_ENCODER_STRUCTURE * jpeg, UINT32 image_format,UINT32 image_width, UINT32 image_height) 
{
	UINT16 mcu_width, mcu_height, bytes_per_pixel;
	lcode = 0;
	bitindex = 0;

	//MCU height 8 width 16
	jpeg->mcu_width = mcu_width = 16;
	//水平方向mcu个数（向上取整）
    jpeg->horizontal_mcus = (UINT16) ((image_width + mcu_width - 1) >> 4);
	jpeg->mcu_height = mcu_height = 8;
	//垂直方向mcu个数（向上取整）
	jpeg->vertical_mcus = (UINT16) ((image_height + mcu_height - 1) >> 3);
	
	//每个像素需要2B YCbYCr4B表示2个像素，因此2B表示1个像素
	bytes_per_pixel = 2;
	
	//按照YCbCr422方式读取mcu
	read_format = read_422_format;
   
	//垂直方向最后一行mcu包含的有效行数
	jpeg->rows_in_bottom_mcus = (UINT16) (image_height - (jpeg->vertical_mcus - 1) * mcu_height);
	//水平方向最后一列mcu包含的有效列数
	jpeg->cols_in_right_mcus = (UINT16) (image_width - (jpeg->horizontal_mcus - 1) * mcu_width);
	
	//由于图像是一维存储的，在实际读取数据中，当切换mcu行时需要给指针加一个偏移量，使得指针始终能够正确指向所要读取的位置
	//如果当前读取的不是水平方向最后一个mcu时所需的偏移量
	jpeg->length_minus_mcu_width = (UINT16) ((image_width - mcu_width) * bytes_per_pixel);
	//如果当前读取的是水平方向最后一个mcu时所需的偏移量
	jpeg->length_minus_width = (UINT16) ((image_width - jpeg->cols_in_right_mcus) * bytes_per_pixel);
	//mcu中一行所需的字节（B）数
	jpeg->mcu_width_size = (UINT16) (mcu_width * bytes_per_pixel);
	//完整编码一行mcu后指针的偏移量 当编码完一行mcu后input_ptr指向第二行的首个mcu的
	//第(mcu_width - jpeg->cols_in_right_mcus)字节需要偏移(mcu_height - 1)行 然后再向左偏移(mcu_width - jpeg->cols_in_right_mcus)个字节
	jpeg->offset =(UINT16) ((image_width * (mcu_height - 1 ) - (mcu_width - jpeg->cols_in_right_mcus)) * bytes_per_pixel);
	 
	jpeg->ldc1 = 0; //Y last dc coefficient Y分量上一个DC（直流）系数
	jpeg->ldc2 = 0; //Cb last dc coefficient
	jpeg->ldc3 = 0; //Cr last dc coefficient
}


/* Multiply Quantization table with quality factor to get LQT and CQT 
使用quality_factor初始化量化矩阵，quality_factor决定数据经量化后0的多少，
quality_factor越大则经量化后数据中0个数越多，压缩比越高
*/ 
void initialize_quantization_tables (UINT32 quality_factor) 
{
	UINT16 i, index;
	UINT32 value;

	//Y分量 量化表
	UINT8 luminance_quant_table[] = {
		16, 11, 10, 16, 24,  40,  51,  61,
		12, 12, 14, 19, 26,  58,  60,  55,
		14, 13, 16, 24, 40,  57,  69,  56,
		14, 17, 22, 29, 51,  87,  80,  62,
		18, 22, 37, 56, 68,  109, 103, 77,
		24, 35, 55, 64, 81,  104, 113, 92,
		49, 64, 78, 87, 103, 121, 120, 101,
		72, 92, 95, 98, 112, 100, 103, 99
	};

	//色度分量（Cb、Cr）量化表
	UINT8 chrominance_quant_table[] = {
		17, 18, 24, 47, 99, 99, 99, 99, 
		18, 21, 26, 66, 99, 99, 99, 99,
		24, 26, 56, 99, 99, 99, 99, 99,
		47, 66, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99, 
		99, 99, 99, 99, 99, 99, 99, 99 
	};

	//量化表按照zigzag（z字形）排列
	for (i = 0; i < 64; i++)
	{
		index = zigzag_table[i];
		/* luminance quantization table * quality factor */ 
		value = luminance_quant_table[i] * quality_factor;
		value = (value + 0x200) >> 10;
		if (value == 0)
		{
			value = 1;
		}
		else if (value > 255) 
		{
			value = 255;
		}

		Lqt[index] = (UINT8) value;
		ILqt[i] = DSP_Division (0x8000, value);
		
		/* chrominance quantization table * quality factor */ 
		value = chrominance_quant_table[i] * quality_factor;
		value = (value + 0x200) >> 10;
		  
		if (value == 0)
		{
		  value = 1;
		}
		else if (value > 255)
		{
		  value = 255;
		}

		Cqt[index] = (UINT8) value;
		ICqt[i] = DSP_Division(0x8000, value);
	 }
}

/*
output_ptr 输出图像指针
处理剩余未序列化的数据，写入JPEG尾部表示信息，返回图像尾部指针
*/
UINT8 *close_bitstream(UINT8 * output_ptr) 
{
	UINT16 i, count;
	UINT8 * ptr;

	//（PUTBITS后）还有未处理完的数据
	if (bitindex > 0)
	{
		lcode <<= (32 - bitindex);
		count = (bitindex + 7) >> 3; //((bitindex + 7) / 8) 向下取整（即，bitindex / 8 向上取整）最后所剩数据的字节数
		ptr = (UINT8 *) & lcode + 3;
		
		for (i = count; i > 0; i--)
		{
			//根据JPEG文档规定，如果数据为0xff需要补0
			if ((*output_ptr++ = *ptr--) == 0xff)
			{
				*output_ptr++ = 0;
			}
		}
	}
 
	// End of image marker
	*output_ptr++ = 0xFF;
	*output_ptr++ = 0xD9;
	return output_ptr;
}


/* 离散余弦变换 DCT for One block(8x8)
|data	|	待处理的数据	|
*/ 
static void DCT (INT16 * data) 
{
	UINT16 i;
	INT32 x0, x1, x2, x3, x4, x5, x6, x7, x8;
  
	/* All values are shifted left by 10
	and rounded off to nearest integer */ 
	static const UINT16 c1 = 1420; /* cos PI/16 * root(2)  */
	static const UINT16 c2 = 1338; /* cos PI/8 * root(2)   */
	static const UINT16 c3 = 1204; /* cos 3PI/16 * root(2) */
	static const UINT16 c5 = 805; /* cos 5PI/16 * root(2) */
	static const UINT16 c6 = 554; /* cos 3PI/8 * root(2)  */
	static const UINT16 c7 = 283; /* cos 7PI/16 * root(2) */
	static const UINT16 s1 = 3;
	static const UINT16 s2 = 10;
	static const UINT16 s3 = 13;
  
	for (i = 8; i > 0; i--)   
    {
		x8 = data[0] + data[7];
		x0 = data[0] - data[7];
		x7 = data[1] + data[6];
		x1 = data[1] - data[6];
		x6 = data[2] + data[5];
		x2 = data[2] - data[5];
		x5 = data[3] + data[4];
		x3 = data[3] - data[4];
		x4 = x8 + x5;
		x8 -= x5;
		x5 = x7 + x6;
		x7 -= x6;
		data[0] = (INT16) (x4 + x5);
		data[4] = (INT16) (x4 - x5);
		data[2] = (INT16) ((x8 * c2 + x7 * c6) >> s2);
		data[6] = (INT16) ((x8 * c6 - x7 * c2) >> s2);
		data[7] = (INT16) ((x0 * c7 - x1 * c5 + x2 * c3 - x3 * c1) >> s2);
		data[5] = (INT16) ((x0 * c5 - x1 * c1 + x2 * c7 + x3 * c3) >> s2);
		data[3] = (INT16) ((x0 * c3 - x1 * c7 - x2 * c1 - x3 * c5) >> s2);
		data[1] = (INT16) ((x0 * c1 + x1 * c3 + x2 * c5 + x3 * c7) >> s2);
		data += 8;
    }

	data -= 64;
	for (i = 8; i > 0; i--)
    {
		x8 = data[0] + data[56];
		x0 = data[0] - data[56];
		x7 = data[8] + data[48];
		x1 = data[8] - data[48];
		x6 = data[16] + data[40];
		x2 = data[16] - data[40];
		x5 = data[24] + data[32];
		x3 = data[24] - data[32];
		x4 = x8 + x5;
		x8 -= x5;
		x5 = x7 + x6;
		x7 -= x6;
		data[0] = (INT16) ((x4 + x5) >> s1);
		data[32] = (INT16) ((x4 - x5) >> s1);
		data[16] = (INT16) ((x8 * c2 + x7 * c6) >> s3);
		data[48] = (INT16) ((x8 * c6 - x7 * c2) >> s3);
		data[56] = (INT16) ((x0 * c7 - x1 * c5 + x2 * c3 - x3 * c1) >> s3);
		data[40] = (INT16) ((x0 * c5 - x1 * c1 + x2 * c7 + x3 * c3) >> s3);
		data[24] = (INT16) ((x0 * c3 - x1 * c7 - x2 * c1 - x3 * c5) >> s3);
		data[8] = (INT16) ((x0 * c1 + x1 * c3 + x2 * c5 + x3 * c7) >> s3);
		data++;
    }
}


/* multiply DCT Coefficients with Quantization table and store in ZigZag location 
对DCT系数进行量化，然后将量化系数按照zigzag排序方式存储（z字形存储）
量化&zigzag存储量化系数

|data				|	待量化的DCT系数	|
|quant_table_ptr	|	量化表指针		|
*/ 
void quantization(INT16 * const data, UINT16 * const quant_table_ptr) 
{
  INT16 i;
  INT32 value;
  for (i = 63; i >= 0; i--)
    
    {
      value = data[i] * quant_table_ptr[i];
      value = (value + 0x4000) >> 15;
      Temp[zigzag_table[i]] = (INT16) value;
    }
}


/*huffman编码
|jpeg_encoder_structure		|	JPEG编码器											|
|component					|	待编码分量类型 1：Y分量 2：Cb分量 others：Cr分量	|
|output_ptr					|	输出图像指针										|
对上一步量化后的数据进行编码，并序列化
*/
UINT8 * huffman(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT16 component, UINT8 * output_ptr)
{
	UINT16 i;
	UINT16 * DcCodeTable, *DcSizeTable, *AcCodeTable, *AcSizeTable;
	INT16 * Temp_Ptr, Coeff, LastDc;

	//RunLength bits编码中下一个非零的系数前零的个数
	UINT16 AbsCoeff, HuffCode, HuffSize, RunLength = 0, DataSize = 0, index;
	INT16 bits_in_next_word;
	UINT16 numbits;
	UINT32 data;

	//zigzag排列的量化后的量化系数表
	Temp_Ptr = Temp;
	//DC量化系数
	Coeff = *Temp_Ptr++;

	//对亮度（Y）分量像素组进行编码
	if (component == 1)
	{
		DcCodeTable = luminance_dc_code_table;
		DcSizeTable = luminance_dc_size_table;
		AcCodeTable = luminance_ac_code_table;
		AcSizeTable = luminance_ac_size_table;
		LastDc = jpeg_encoder_structure->ldc1;
		jpeg_encoder_structure->ldc1 = Coeff;
	}
	//对其他两个分量（Cb、Cr）分量像素组进行编码
	else
	{
		DcCodeTable = chrominance_dc_code_table;
		DcSizeTable = chrominance_dc_size_table;
		AcCodeTable = chrominance_ac_code_table;
		AcSizeTable = chrominance_ac_size_table;

		//Cb
		if (component == 2)
		{
			LastDc = jpeg_encoder_structure->ldc2;
			jpeg_encoder_structure->ldc2 = Coeff;
		}
		//Cr
		else
		{
			LastDc = jpeg_encoder_structure->ldc3;
			jpeg_encoder_structure->ldc3 = Coeff;
		}
	}


	//Coeff 与LastDc的差 AbsCoeff 与LastDc的差的绝对值
	Coeff -= LastDc;
	AbsCoeff = (Coeff < 0) ? -Coeff-- : Coeff;
  
	//统计AbsCoeff二进制位数
	while(AbsCoeff != 0)
    {
		AbsCoeff >>= 1;
		DataSize++;
    }


	//对DC系数进行编码
	//bit编码的前两位
	HuffCode = DcCodeTable[DataSize];
	HuffSize = DcSizeTable[DataSize];
	Coeff &= (1 << DataSize) - 1;//处理负数 Coeff为对应Huffman编码 长度就是该数值的有效数据位个数，编码值就是其本身；
								//而对于负数，则需要以有效数据位长度（不包含符号位）为满量程，减去实际负数值为编码值。
								//例如，-7，-3，-1的编码值都为0，而-6，-2的编码值为1。这也就刚好对应了jpeg标准码表
	data = (HuffCode << DataSize) | Coeff; 
	numbits = HuffSize + DataSize;
  
	//PUTBITS
	{ 
		bits_in_next_word = (INT16) (bitindex + numbits - 32); 
		if (bits_in_next_word < 0) 
		{ 
			lcode = (lcode << numbits) | data; 
			bitindex += numbits; 
		} 
		else 
		{ 
			lcode = (lcode << (32 - bitindex)) | (data >> bits_in_next_word); 
			if ((*output_ptr++ = (UINT8)(lcode >> 24)) == 0xff) 
			{
				*output_ptr++ = 0;
			}
			
			if ((*output_ptr++ = (UINT8)(lcode >> 16)) == 0xff) 
			{
				*output_ptr++ = 0;
			}

			if ((*output_ptr++ = (UINT8)(lcode >> 8)) == 0xff) 
			{
				*output_ptr++ = 0;
			}


			if ((*output_ptr++ = (UINT8)lcode) == 0xff) 
			{
				*output_ptr++ = 0;
			}
			lcode = data; 
			bitindex = bits_in_next_word; 
		} 
    }  
  
	//对剩余AC系数进行编码
	for (i = 63; i > 0; i--)
    {
		if ((Coeff = *Temp_Ptr++) != 0)
		{
			//如果当前系数前有超过15个0 最多以16个字符为一组
			while (RunLength > 15) 
			{
				RunLength -= 16;
				data = AcCodeTable[161];
				numbits = AcSizeTable[161];
				//PUTBITS
				{ 
					bits_in_next_word = (INT16) (bitindex + numbits - 32); 
					if (bits_in_next_word < 0) 
					{ 
						lcode = (lcode << numbits) | data; 
						bitindex += numbits; 
					} 
					else 
					{ 
						lcode = (lcode << (32 - bitindex)) | (data >> bits_in_next_word); 
						if ((*output_ptr++ = (UINT8)(lcode >> 24)) == 0xff) 
						{
							*output_ptr++ = 0;
						}
						if ((*output_ptr++ = (UINT8)(lcode >> 16)) == 0xff)
						{
							*output_ptr++ = 0;
						}
						if ((*output_ptr++ = (UINT8)(lcode >> 8)) == 0xff)
						{
							*output_ptr++ = 0;
						}
						if ((*output_ptr++ = (UINT8)lcode) == 0xff)
						{
							*output_ptr++ = 0;
						}
						lcode = data; 
						bitindex = bits_in_next_word; 
					} 
				}    
			}
   
			AbsCoeff = (Coeff < 0) ? -Coeff-- : Coeff;

			//AC系数 编码所需bit数
			if (AbsCoeff >> 8 == 0) // <= 255
			{
				DataSize = bitsize[AbsCoeff];
			}
			else 
			{
				DataSize = bitsize[AbsCoeff >> 8] + 8;
			}
   
			//example: (3,3) 前面有三个0，后面AC系数为3bit 直接使用AcCodeTable AcSizeTable查找即可 (15,0)表示16个0 
			index = RunLength * 10 + DataSize;
			HuffCode = AcCodeTable[index];
			HuffSize = AcSizeTable[index];
			Coeff &= (1 << DataSize) - 1;
			data = (HuffCode << DataSize) | Coeff;
			numbits = HuffSize + DataSize;
			
			// PUTBITS
			{ 
				//bits_in_next_word data应该在下一个字的位数
				bits_in_next_word = (INT16) (bitindex + numbits - 32); 
				if (bits_in_next_word < 0) 
				{ 
					lcode = (lcode << numbits) | data; 
					bitindex += numbits; 
				} 
				else 
				{ 
					//始终保持lcode中存储一个字的数据 如果lcode 某个字节为0xff 需要在之后填充0x00
					lcode = (lcode << (32 - bitindex)) | (data >> bits_in_next_word); 
					if ((*output_ptr++ = (UINT8)(lcode >> 24)) == 0xff)
					{
						*output_ptr++ = 0;
					}

					if ((*output_ptr++ = (UINT8)(lcode >> 16)) == 0xff)
					{
						*output_ptr++ = 0;
					}
					
					if ((*output_ptr++ = (UINT8)(lcode >> 8)) == 0xff)
					{
						*output_ptr++ = 0;
					}
					
					if ((*output_ptr++ = (UINT8)lcode) == 0xff)
					{
						*output_ptr++ = 0;
					}
					lcode = data; 
					bitindex = bits_in_next_word; 
				} 
			}    
			RunLength = 0;
		}
		else 
		{
			RunLength++;
		}
    }
  
	//后面全是0（EOB End of bits）
	if (RunLength != 0)
    {
		data = AcCodeTable[0];
		numbits = AcSizeTable[0];
	  
		// PUTBITS 
		{ 
			bits_in_next_word = (INT16) (bitindex + numbits - 32); 
			if (bits_in_next_word < 0) 
			{ 
				lcode = (lcode << numbits) | data; 
				bitindex += numbits; 
			} 
			else 
			{ 
				lcode = (lcode << (32 - bitindex)) | (data >> bits_in_next_word); 
				if ((*output_ptr++ = (UINT8)(lcode >> 24)) == 0xff)
				{
					*output_ptr++ = 0;
				}
				
				if ((*output_ptr++ = (UINT8)(lcode >> 16)) == 0xff)
				{
					*output_ptr++ = 0;
				}
    
				if ((*output_ptr++ = (UINT8)(lcode >> 8)) == 0xff)
				{
					*output_ptr++ = 0;
				}
    
				if ((*output_ptr++ = (UINT8)lcode) == 0xff)
				{
					*output_ptr++ = 0;
				}
				lcode = data; 
				bitindex = bits_in_next_word; 
			} 
		}  
    }
	return output_ptr;
}

/*
对一个MCU单元进行编码

|jpeg_encoder_structure		|	JPEG编码器		|
|image_format				|	待编码格式		|
|output_ptr					|	输出图像指针	|

编码步骤：DCT变换、量化、huffman编码（包含数据序列化）
*/
static UINT8 *encodeMCU(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,UINT32 image_format, UINT8 * output_ptr) 
{
	DCT(Y1);
	quantization(Y1, ILqt);
	output_ptr = huffman(jpeg_encoder_structure, 1, output_ptr);

	if (image_format == FOUR_ZERO_ZERO) 
	{
		return output_ptr;
	}
  
	DCT(Y2);
	quantization(Y2, ILqt);
	output_ptr = huffman(jpeg_encoder_structure, 1, output_ptr);
	
	if (image_format == FOUR_TWO_TWO) 
	{
		goto chroma;
	}
  
	DCT(Y3);
	quantization(Y3, ILqt);
	output_ptr = huffman(jpeg_encoder_structure, 1, output_ptr);
  
	DCT(Y4);
	quantization(Y4, ILqt);
	output_ptr = huffman(jpeg_encoder_structure, 1, output_ptr);

	chroma:DCT(CB);
		quantization (CB, ICqt);
		output_ptr = huffman (jpeg_encoder_structure, 2, output_ptr);
		DCT (CR);
		quantization (CR, ICqt);
		output_ptr = huffman (jpeg_encoder_structure, 3, output_ptr);
	return output_ptr;
}


/*
jpeg编码encode实现

|input_ptr		|	输入图像	|
|output_ptr		|	输出图像	|
|quality_factor |	图像质量	|
|image_format	|	图像格式	|
|image_width	|	图像width	|
|image_height	|	图像height	|
*/
UINT32 encode_image (UINT8 * input_ptr, UINT8 * output_ptr, UINT32 quality_factor, UINT32 image_format, UINT32 image_width, UINT32 image_height) 
{
	UINT16 i, j;
	UINT8 * output;
	JPEG_ENCODER_STRUCTURE JpegStruct;
	JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure = &JpegStruct;
	output = output_ptr;
	image_format = FOUR_TWO_TWO;
	RGB_2_422(input_ptr, output_ptr, image_width, image_height);
	/* Initialization of JPEG control structure */ 
	initialization(jpeg_encoder_structure, image_format, image_width, image_height);
	/* Quantization Table Initialization */ 
	initialize_quantization_tables(quality_factor);
	/* Writing Marker Data */ 
	output_ptr = write_markers(output_ptr, image_format, image_width, image_height);

	for (i = 1; i <= jpeg_encoder_structure->vertical_mcus; i++)
	{
		//不是垂直方向最后一个mcu
		if (i < jpeg_encoder_structure->vertical_mcus) 
		{
			jpeg_encoder_structure->rows = jpeg_encoder_structure->mcu_height;
		}
		//是垂直方向最后一个mcu
		else 
		{
			jpeg_encoder_structure->rows = jpeg_encoder_structure->rows_in_bottom_mcus;
		}
		
		
		for (j = 1; j <= jpeg_encoder_structure->horizontal_mcus; j++)
		{
			//不是水平方向最后一个mcu
			if (j < jpeg_encoder_structure->horizontal_mcus)
			{
				jpeg_encoder_structure->cols = jpeg_encoder_structure->mcu_width;
				jpeg_encoder_structure->incr = jpeg_encoder_structure->length_minus_mcu_width;
			}
			//是水平方向最后一个mcu
			else
			{
				jpeg_encoder_structure->cols = jpeg_encoder_structure->cols_in_right_mcus;
				jpeg_encoder_structure->incr = jpeg_encoder_structure->length_minus_width;
			}

			//一次读取一个mcu
			read_format(jpeg_encoder_structure, input_ptr);
		   
			/* Encode the data in MCU */ 
			output_ptr = encodeMCU(jpeg_encoder_structure, image_format, output_ptr);
			//这里的input_ptr 始终指向待编码的MCU第一行的首个字节
			input_ptr += jpeg_encoder_structure->mcu_width_size;
		}
		//编码完一行MCU,input_ptr所需移动的偏移量
		input_ptr += jpeg_encoder_structure->offset;
	 }
	 /* Close Routine */ 
	 output_ptr = close_bitstream(output_ptr);
	 return (UINT32) (output_ptr - output);
}


/*
jpeg编码入口 

|	widthIn			|	输入图像width								|
|	heightIn		|	输入图像height								|
|	rdImg			|	输入图像									|
|	wrImg			|	输出图像								|
|	quality_factor	|	压缩质量（数值越大压缩比越高，建议[1-3000]）
*/
UINT32 jpeg_main(UINT32 widthIn, UINT32 heightIn, UINT32 quality_factor, unsigned char *rdImg, unsigned char *wrImg)
{
	//int x,y;
	UINT32 sizeOut;
	int sizeIn = widthIn * heightIn * 3;
	unsigned char *inpict ; 
	inpict = (unsigned char *) malloc (sizeIn);
	memcpy(inpict,rdImg,sizeIn);//原始图像
	//for(x=0;x<heightIn;x++)//inpict初始化成RGB模式
	//for (y=0;y<widthIn;y++)
	//{
	//	*(inpict+x*3+y*heightIn*3)=y%2;
	//	*(inpict+x*3+y*heightIn*3+1)=x%2;
	//	*(inpict+x*3+y*heightIn*3+2)=y%2;
	//}

	//选取一张raw测试压缩比1-4 300-10 1060-20 3000-34
	sizeOut = encode_image(inpict,wrImg,quality_factor, FOUR_TWO_TWO,widthIn,heightIn);
	free(inpict);
	return sizeOut;
}