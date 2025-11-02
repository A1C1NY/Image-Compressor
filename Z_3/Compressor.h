#pragma once
#ifndef IMAGE_COMPRESSOR_H
#define IMAGE_COMPRESSOR_H
using namespace std;

#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <map>
#include <queue>
#include <bitset>
#endif

typedef unsigned char BYTE;
typedef unsigned int UINT;


struct HuffmanNode {
	int value;
	int frequency;
	HuffmanNode* left;
	HuffmanNode* right;

	HuffmanNode(int freq, int val = -1) {
		frequency = freq;
		value = val;
		left = nullptr;
		right = nullptr;
	}
};

auto cmp = [](HuffmanNode* a, HuffmanNode* b) {
	if (a->frequency == b->frequency) {
		return a->value > b->value;
	}
	else {
		return a->frequency > b->frequency;
	}
};

struct CompressedImageInfo {
	UINT width;
	UINT height;
	UINT originalWidth; 
	UINT originalHeight;
	int decompositionLevel; 
	double threshold; 
	UINT compressedDataSize;
};

class Compressor {
public:
	bool compressImage(BYTE* originalRgbData, UINT originalWidth, UINT originalHeight,
		const string& outputFilePath, int decompositionLevel = 1, double threshold = 5.0);

	BYTE* decompressImage(string& inputFilePath, UINT& originalWidth, UINT& originalHeight);

	// 一维Haar小波变换及其逆变换
	void haar1D(vector<double>& data, int len);
	static void inverseHaar1D(vector<double>& data, int len);

	// 二维Haar小波变换及其逆变换
	void haar2D(vector<vector<double>>& block);
	void inverseHaar2D(vector<vector<double>>& block);

	// 颜色预处理与后处理
	vector<vector<vector<double>>> preprocessRGBA(BYTE* rgbData, UINT originalWidth,
		UINT originalHeight, UINT& paddedWidth, UINT& paddedHeight);
	BYTE* postprocessRGBA(vector<vector<vector<double>>>& rgbData,
		UINT originalWidth, UINT originalHeight);

	// 量化函数，将过小的系数设为0，接近的系数量化为相同值
	void quantizeRGBA(vector<vector<vector<double>>>& coefficients, double threshold);

	// 将RGBA系数转换为整数序列
	vector<int> RGBAToIntegers(vector<vector<vector<double>>>& coefficients);
	vector<vector<vector<double>>> IntegersToRGBA(vector<int>& integers, UINT width, UINT height);

	// Huffman树的构建
	HuffmanNode* buildHuffmanTree(vector<int>& data);

	// 由Huffman树生成编码表
	void generateCodes(HuffmanNode* root, const string& code, map<int, string>& codes);

	// Huffman编码
	vector<bool> encode(vector<int>& data, map<int, string>& codes);
	vector<int> decode(vector<bool>& encodedData, HuffmanNode* root);

	// Huffman序列写入与读取
	void serialize(HuffmanNode* root, ofstream& ofs);
	HuffmanNode* deserialize(ifstream& ifs);

	void writeCompressedData(ofstream& ofs, vector<bool>& data);
	vector<bool> readCompressedData(ifstream& ifs, UINT dataSize);


	void deleteHuffmanTree(HuffmanNode* root);

};