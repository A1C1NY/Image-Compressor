#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include "PicReader.h"
#include "Compressor.h"
#include <cstdint>
#include <cstring>

using namespace std;

/*
--Jesus save me!!!
--JPG or PNG?
*/


// 另外，由于VS2022逆天的GBK编码中文，所以输出全部使用英语。
bool savePNG(const string& filename, BYTE* data, UINT width, UINT height);
static void write_uint32(ofstream& file, uint32_t val);
static uint32_t crc32(const unsigned char* buf, size_t len);

int main(int argc, char* argv[]) {
	if (argc != 3) {
		cerr << "Wrong number of arguments!" << endl;
		return 1;
	}

	string action = argv[1];
	string inputPath = argv[2];
	string outputPath;

	// 从输入路径中提取不带扩展名的基本文件名
	string base_filename = inputPath.substr(0, inputPath.find_last_of('.'));

	if (action == "-compress") {
		outputPath = base_filename + ".dat";
	}
	else if (action == "-read") {
		outputPath = base_filename + ".png";
	}
	else {
		cerr << "Invalid action: " << action << endl;
		cerr << "Action must be '-compress' or '-read'." << endl;
		return 1;
	}

	int decompositionLevel = 6;
	int threshold = 15;

	PicReader picReader;
	Compressor compressor;

	if (action == "-compress") {
		cout << "Reading image from: " << inputPath << endl;
		picReader.readPic(inputPath.c_str());
		BYTE* originalData = nullptr;
		UINT originalWidth = 0, originalHeight = 0;
		picReader.getData(originalData, originalWidth, originalHeight); // 使用PicReader获取图像数据

		if (originalData == nullptr) { // 读取失败
			cerr << "Failed to read image data." << endl;
			return -1;
		}

		std::cout << "Original image size: " << originalWidth << " x " << originalHeight << std::endl;

		cout << "=====Compressing image=====" << endl;
		if (compressor.compressImage(originalData, originalWidth, originalHeight, outputPath, decompositionLevel, threshold)) {
			cout << "Image compressed successfully to: " << outputPath << endl;
		}
		else {
			cerr << "Image compression failed." << endl;
			return -1;
		}
	}
	else if (action == "-read") {
		cout << "=====Reading compressed image=====" << endl;
		UINT width, height;
		BYTE* decompressedData = compressor.decompressImage(inputPath, width, height);

		if (decompressedData != nullptr) {
			cout << "Decompressed image size: " << width << " x " << height << endl;
			if (savePNG(outputPath, decompressedData, width, height)) {
				std::cout << "Decompressed image saved as: " << outputPath << std::endl;
			}
			picReader.showPic(decompressedData, width, height);
			delete[] decompressedData;
			cout << "Decompression and display successful." << endl;
		}
		else {
			cerr << "Image decompression failed." << endl;
			return -1;
		}

		cout << "Press any key to exit..." << endl;
		cin.get();
		return 0;
	}
}

// 转换PNG实现部分
//PNG chunk（IHDR、IDAT、IEND、……）的完整性校验（类型+数据）。实现PNG/ZIP 等用的反射 CRC-32
static uint32_t crc32(const unsigned char* buf, size_t len) {
	uint32_t crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < len; ++i) {
		crc ^= static_cast<uint32_t>(buf[i]);
		for (int j = 0; j < 8; ++j) {
			uint32_t mask = (crc & 1u) ? 0xEDB88320u : 0u;
			crc = (crc >> 1) ^ mask;
		}
	}
	return ~crc;
}

// 显式的先写大端后写小端, 因为某些编译器可能默认小端
static void write_uint32(std::ofstream& file, uint32_t val) {
	file.put(static_cast<char>((val >> 24) & 0xFF));
	file.put(static_cast<char>((val >> 16) & 0xFF));
	file.put(static_cast<char>((val >> 8) & 0xFF));
	file.put(static_cast<char>(val & 0xFF));
}

// 保存为png格式文件
bool savePNG(const std::string& filename, BYTE* data, UINT width, UINT height) {
	if (!data) return false;

	std::ofstream file(filename, std::ios::binary);
	if (!file.is_open()) return false;

	// PNG文件头
	const unsigned char png_signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	file.write(reinterpret_cast<const char*>(png_signature), 8);

	// IHDR块
	BYTE* ihdr_data = new BYTE[13];
	ihdr_data[0] = static_cast<BYTE>((width >> 24) & 0xFF);
	ihdr_data[1] = static_cast<BYTE>((width >> 16) & 0xFF);
	ihdr_data[2] = static_cast<BYTE>((width >> 8) & 0xFF);
	ihdr_data[3] = static_cast<BYTE>(width & 0xFF);
	ihdr_data[4] = static_cast<BYTE>((height >> 24) & 0xFF);
	ihdr_data[5] = static_cast<BYTE>((height >> 16) & 0xFF);
	ihdr_data[6] = static_cast<BYTE>((height >> 8) & 0xFF);
	ihdr_data[7] = static_cast<BYTE>(height & 0xFF);
	ihdr_data[8] = 8; // 
	ihdr_data[9] = 6; // RGBA
	ihdr_data[10] = 0; // 压缩方式
	ihdr_data[11] = 0; // 滤波器方法
	ihdr_data[12] = 0; // 隔行扫描

	// 写入IHDR块
	write_uint32(file, 13);
	file.write("IHDR", 4);
	file.write(reinterpret_cast<char*>(ihdr_data), 13);

	// 写入CRC校验
	{
		std::vector<unsigned char> crcbuf;
		crcbuf.insert(crcbuf.end(), { 'I','H','D','R' });
		crcbuf.insert(crcbuf.end(), ihdr_data, ihdr_data + 13);
		uint32_t crc = crc32(crcbuf.data(), crcbuf.size());
		write_uint32(file, crc);
	}

	//  转化为扫描行形式
	std::vector<unsigned char> raw;
	raw.reserve(static_cast<size_t>(height) * (width * 4 + 1)); // 提前声明空间大小
	for (UINT y = 0; y < height; ++y) {
		raw.push_back(0); // 没有采用滤波器
		for (UINT x = 0; x < width; ++x) {
			size_t src_idx = static_cast<size_t>(y) * width * 4 + static_cast<size_t>(x) * 4;
			raw.push_back(data[src_idx]);       // R
			raw.push_back(data[src_idx + 1]);   // G
			raw.push_back(data[src_idx + 2]);   // B
			raw.push_back(data[src_idx + 3]);   // A
		}
	}

	// 构建IDAT块，这里其实没有压缩，直接将还原出来的全部数据转化后写入，所以png还原了大部分数据，
	// 当然，通过一些巧妙的设计，使得图片查看器能正确显示图片
	// 另外由于加上了各种校验，文件会比原始数据大一些
	std::vector<unsigned char> idat;
	idat.push_back(0x78); // CMF 
	idat.push_back(0x01); // FLG 

	const size_t MAX_BLOCK = 65535; // 一个DEFLATE 数据块
	size_t offset = 0;
	while (offset < raw.size()) {
		size_t remaining = raw.size() - offset;// 每一轮写完的剩余数据
		uint16_t block_size = static_cast<uint16_t>(min(remaining, MAX_BLOCK)); // 这一轮写入的大小

		bool is_last = (offset + block_size == raw.size());

		// 写入块头
		unsigned char bfinal_btype = is_last ? 0x01u : 0x00u; // 分辨最后一个块，00表示未压缩
		idat.push_back(bfinal_btype);

		// 写入块大小和其补码
		idat.push_back(static_cast<unsigned char>(block_size & 0xFF)); // 低字节在前
		idat.push_back(static_cast<unsigned char>((block_size >> 8) & 0xFF)); // 高字节在后
		uint16_t nlen = static_cast<uint16_t>(~block_size);
		idat.push_back(static_cast<unsigned char>(nlen & 0xFF));
		idat.push_back(static_cast<unsigned char>((nlen >> 8) & 0xFF));

		idat.insert(idat.end(), raw.begin() + offset, raw.begin() + offset + block_size);
		offset += block_size;
	}

	// 在IDAT块后添加Adler-32校验码
	{
		uint32_t s1 = 1;
		uint32_t s2 = 0;
		for (unsigned char byte : raw) {
			s1 = (s1 + byte) % 65521u;
			s2 = (s2 + s1) % 65521u;
		}
		uint32_t adler32 = (s2 << 16) | s1;
		idat.push_back(static_cast<unsigned char>((adler32 >> 24) & 0xFF));
		idat.push_back(static_cast<unsigned char>((adler32 >> 16) & 0xFF));
		idat.push_back(static_cast<unsigned char>((adler32 >> 8) & 0xFF));
		idat.push_back(static_cast<unsigned char>(adler32 & 0xFF));
	}

	// 写入IDAT块
	write_uint32(file, static_cast<uint32_t>(idat.size()));
	file.write("IDAT", 4);
	file.write(reinterpret_cast<const char*>(idat.data()), idat.size());
	{
		std::vector<unsigned char> crcbuf;
		crcbuf.insert(crcbuf.end(), { 'I','D','A','T' });
		crcbuf.insert(crcbuf.end(), idat.begin(), idat.end());
		uint32_t crc = crc32(crcbuf.data(), crcbuf.size());
		write_uint32(file, crc);
	}

	// IEND块
	write_uint32(file, 0);
	file.write("IEND", 4);
	{
		uint32_t crc = crc32(reinterpret_cast<const unsigned char*>("IEND"), 4);
		write_uint32(file, crc);
	}

	file.close();
	return true;
}
