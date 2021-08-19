// JPEG_Encode.cpp : 定义控制台应用程序的入口点。
//

//#include "afxdialogex.h"
#include "stdafx.h"
#include "jepg.h"

UINT32 sizeOut;		//压缩后的尺寸
//UINT32 image_width = 2448;		//宽
//UINT32 image_height = 2048;
UINT32 image_width = 1920;		//宽
UINT32 image_height = 1080;		//高
unsigned char *bmpImg24R;	//24位图像数据
unsigned char *img_raw = new unsigned char[image_width*image_height];
unsigned char *img_rgb = new unsigned char[image_width*image_height*3];
unsigned char *jpegImg = new unsigned char[image_width*image_height * 3];		//要写的图像数据

bool readRAW(void)
{
	FILE *fp = fopen("003.raw", "rb");
	char* buf = new char[image_width * image_height];
	fread(buf, 1, image_width * image_height, fp);
	fclose(fp);
	for (int i = 0; i < image_height - 1; i++)
		memcpy(img_raw + image_width * i, buf + image_width * i + 1, image_width - 1);
	return 1;
}

void bayerWB(unsigned char *src, unsigned char *des, int wid, int hei, const char* mode)
{
	float wbRatioR = 1.5;
	float wbRatioG = 1.0;
	float wbRatioB = 1.5;

	int value;
	for (int i = 0; i < hei - 1; i++)
		for (int j = 0; j < wid - 1; j++)
		{
			value = src[i * wid + j];

			if (mode == "GBRG")
			{
				//偶行
				if (i % 2 == 0)
				{
					//G
					if (j % 2 == 0)
					{
						if (value*wbRatioG > 255)
							value = 255;
						else
							value *= wbRatioG;
					}
					//B
					else
					{
						if (value*wbRatioB > 255)
							value = 255;
						else
							value *= wbRatioB;
					}
				}
				else
				{
					//R
					if (j % 2 == 0)
					{
						if (value*wbRatioR > 255)
							value = 255;
						else
							value *= wbRatioR;
					}
					//G
					else
					{
						if (value*wbRatioG > 255)
							value = 255;
						else
							value *= wbRatioG;
					}
				}
			}
			src[i * wid + j] = value;
		}
	for (int i = 1; i < hei - 2; i++)
		for (int j = 1; j < wid - 2; j++)
		{
			if (mode == "GBRG")
			{
				//偶行
				if (i % 2 == 0)
				{
					//G
					if (j % 2 == 0)
					{
						des[i * wid * 3 + j * 3 + 0] = src[(i + 1) * wid + j];
						des[i * wid * 3 + j * 3 + 1] = src[i * wid + j];
						des[i * wid * 3 + j * 3 + 2] = src[i * wid + j - 1];
					}
					//B
					else
					{
						des[i * wid * 3 + j * 3 + 0] = src[(i + 1) * wid + j + 1];
						des[i * wid * 3 + j * 3 + 1] = src[i * wid + j - 1];
						des[i * wid * 3 + j * 3 + 2] = src[i * wid + j];
					}
				}
				else
				{
					//R
					if (j % 2 == 0)
					{
						des[i * wid * 3 + j * 3 + 0] = src[i * wid + j];
						des[i * wid * 3 + j * 3 + 1] = src[(i - 1) * wid + j];
						des[i * wid * 3 + j * 3 + 2] = src[(i - 1) * wid + j + 1];
					}
					//G
					else
					{
						des[i * wid * 3 + j * 3 + 0] = src[i * wid + j - 1];
						des[i * wid * 3 + j * 3 + 1] = src[i * wid + j];
						des[i * wid * 3 + j * 3 + 2] = src[(i - 1) * wid + j];
					}
				}
			}
		}
}

bool writeJPEG(void)
{
	FILE *fp = fopen("3_ys.jpg", "wb+");   //以二进制写的方式打开文件
	if (fp == 0) return 0;
	fwrite(jpegImg, sizeOut, 1, fp);    //写jpeg
	fclose(fp);
	return 1;
}

int main()
{
	readRAW();
	bayerWB(img_raw, img_rgb, image_width, image_height, "GBRG");
	sizeOut = jpeg_main(image_width, image_height, 1, img_rgb, jpegImg);
	writeJPEG();
    return 0;
}

