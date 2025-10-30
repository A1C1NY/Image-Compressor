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


// ���⣬����VS2022�����GBK�������ģ��������ȫ��ʹ��Ӣ�
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

	// ������·������ȡ������չ���Ļ����ļ���
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
		picReader.getData(originalData, originalWidth, originalHeight); // ʹ��PicReader��ȡͼ������

		if (originalData == nullptr) { // ��ȡʧ��
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

// ת��PNGʵ�ֲ���
//PNG chunk��IHDR��IDAT��IEND����������������У�飨����+���ݣ���ʵ��PNG/ZIP ���õķ��� CRC-32
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

// ��ʽ����д��˺�дС��, ��ΪĳЩ����������Ĭ��С��
static void write_uint32(std::ofstream& file, uint32_t val) {
	file.put(static_cast<char>((val >> 24) & 0xFF));
	file.put(static_cast<char>((val >> 16) & 0xFF));
	file.put(static_cast<char>((val >> 8) & 0xFF));
	file.put(static_cast<char>(val & 0xFF));
}

// ����Ϊpng��ʽ�ļ�
bool savePNG(const std::string& filename, BYTE* data, UINT width, UINT height) {
	if (!data) return false;

	std::ofstream file(filename, std::ios::binary);
	if (!file.is_open()) return false;

	// PNG�ļ�ͷ
	const unsigned char png_signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	file.write(reinterpret_cast<const char*>(png_signature), 8);

	// IHDR��
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
	ihdr_data[10] = 0; // ѹ����ʽ
	ihdr_data[11] = 0; // �˲�������
	ihdr_data[12] = 0; // ����ɨ��

	// д��IHDR��
	write_uint32(file, 13);
	file.write("IHDR", 4);
	file.write(reinterpret_cast<char*>(ihdr_data), 13);

	// д��CRCУ��
	{
		std::vector<unsigned char> crcbuf;
		crcbuf.insert(crcbuf.end(), { 'I','H','D','R' });
		crcbuf.insert(crcbuf.end(), ihdr_data, ihdr_data + 13);
		uint32_t crc = crc32(crcbuf.data(), crcbuf.size());
		write_uint32(file, crc);
	}

	//  ת��Ϊɨ������ʽ
	std::vector<unsigned char> raw;
	raw.reserve(static_cast<size_t>(height) * (width * 4 + 1)); // ��ǰ�����ռ��С
	for (UINT y = 0; y < height; ++y) {
		raw.push_back(0); // û�в����˲���
		for (UINT x = 0; x < width; ++x) {
			size_t src_idx = static_cast<size_t>(y) * width * 4 + static_cast<size_t>(x) * 4;
			raw.push_back(data[src_idx]);       // R
			raw.push_back(data[src_idx + 1]);   // G
			raw.push_back(data[src_idx + 2]);   // B
			raw.push_back(data[src_idx + 3]);   // A
		}
	}

	// ����IDAT�飬������ʵû��ѹ����ֱ�ӽ���ԭ������ȫ������ת����д�룬����png��ԭ�˴󲿷����ݣ�
	// ��Ȼ��ͨ��һЩ�������ƣ�ʹ��ͼƬ�鿴������ȷ��ʾͼƬ
	// �������ڼ����˸���У�飬�ļ����ԭʼ���ݴ�һЩ
	std::vector<unsigned char> idat;
	idat.push_back(0x78); // CMF 
	idat.push_back(0x01); // FLG 

	const size_t MAX_BLOCK = 65535; // һ��DEFLATE ���ݿ�
	size_t offset = 0;
	while (offset < raw.size()) {
		size_t remaining = raw.size() - offset;// ÿһ��д���ʣ������
		uint16_t block_size = static_cast<uint16_t>(min(remaining, MAX_BLOCK)); // ��һ��д��Ĵ�С

		bool is_last = (offset + block_size == raw.size());

		// д���ͷ
		unsigned char bfinal_btype = is_last ? 0x01u : 0x00u; // �ֱ����һ���飬00��ʾδѹ��
		idat.push_back(bfinal_btype);

		// д����С���䲹��
		idat.push_back(static_cast<unsigned char>(block_size & 0xFF)); // ���ֽ���ǰ
		idat.push_back(static_cast<unsigned char>((block_size >> 8) & 0xFF)); // ���ֽ��ں�
		uint16_t nlen = static_cast<uint16_t>(~block_size);
		idat.push_back(static_cast<unsigned char>(nlen & 0xFF));
		idat.push_back(static_cast<unsigned char>((nlen >> 8) & 0xFF));

		idat.insert(idat.end(), raw.begin() + offset, raw.begin() + offset + block_size);
		offset += block_size;
	}

	// ��IDAT������Adler-32У����
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

	// д��IDAT��
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

	// IEND��
	write_uint32(file, 0);
	file.write("IEND", 4);
	{
		uint32_t crc = crc32(reinterpret_cast<const unsigned char*>("IEND"), 4);
		write_uint32(file, crc);
	}

	file.close();
	return true;
}
