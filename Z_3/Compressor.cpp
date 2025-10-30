#include "Compressor.h"  
#include <iostream>  
#include <cstring>  
#include <climits>  
using namespace std;

/*  
  _ooOoo_  
 (  -_-  )  
  /  |  \  
 /| \_/ |\  
    / \/  
   /___\  
永不报错 永不出Bug

其实使用字符画是最有效的图像压缩方法  
*/  

// 压缩函数流程
bool Compressor::compressImage(BYTE* originalRgbData, UINT originalWidth, UINT originalHeight,
	const string& outputFilePath, int decompositionLevel, double threshold) {
	if (!originalRgbData) {
		cerr << "Can not compress null data!" << endl;
		return false;
	}

	UINT paddedWidth, paddedHeight;

	vector<vector<vector<double>>> rgbData = preprocessRGBA(originalRgbData, originalWidth, originalHeight, paddedWidth, paddedHeight);

	cout << "Compression Info:\n";
	cout << "Original size: " << originalWidth << " x " << originalHeight << endl;
	cout << "Padded size:" << paddedWidth << " x " << paddedHeight << endl;
	cout << "Decomposition levels: " << decompositionLevel << endl;
	cout << "Threshold: " << threshold << endl;

	// 对每个通道进行Haar小波变换
	for (int c = 0; c < 4; ++c) {
		cout << "Processing channel..." << endl;

		// 读取一个通道的数据
		vector<vector<double>> channelData(paddedHeight, vector<double>(paddedWidth, 0.0));
		for (UINT y = 0; y < paddedHeight; ++y) {
			for (UINT x = 0; x < paddedWidth; ++x) {
				channelData[y][x] = rgbData[y][x][c];
			}
		}
		// 多层Haar变换
		for (int level = 0; level < decompositionLevel; ++level) {
			haar2D(channelData);
		}

		// 变化后的数据传回rgbData
		for (UINT y = 0; y < paddedHeight; ++y) {
			for (UINT x = 0; x < paddedWidth; ++x) {
				rgbData[y][x][c] = channelData[y][x];
			}
		}
	} 

	// 利用量化函数进行量化处理
	quantizeRGBA(rgbData, threshold);

	// 放大为整数
	vector<int> integerCoefficients = RGBAToIntegers(rgbData);

	// 构建Huffman树
	HuffmanNode* huffmanTree = buildHuffmanTree(integerCoefficients);
	map<int, std::string> huffmanCodes;
	generateCodes(huffmanTree, "", huffmanCodes);
	cout << "Generated " << huffmanCodes.size() << " Huffman codes." << endl;

	// 编码
	vector<bool> encodedData = encode(integerCoefficients, huffmanCodes);
	cout << "Encoded data size (in bits): " << encodedData.size() << endl;

	// 写入文件
	std::ofstream ofs(outputFilePath, ios::binary);
	if (!ofs.is_open()) {
		cerr << "Error: Could not open output file " << outputFilePath << " for writing." << endl;
		deleteHuffmanTree(huffmanTree);
		return false;
	}

	CompressedImageInfo info;
	info.width = paddedWidth;
	info.height = paddedHeight;
	info.originalWidth = originalWidth;
	info.originalHeight = originalHeight;
	info.decompositionLevel = decompositionLevel;
	info.threshold = threshold;
	info.compressedDataSize = encodedData.size();

	ofs.write(reinterpret_cast<const char*>(&info), sizeof(CompressedImageInfo));


	serialize(huffmanTree, ofs);
	writeCompressedData(ofs, encodedData);

	ofs.close();
	deleteHuffmanTree(huffmanTree);

	cout << "Compression completed successfully to " << outputFilePath << endl;
	return true;
}

// 解压缩函数流程
BYTE* Compressor::decompressImage(string& inputFilePath, UINT& originalWidth, UINT& originalHeight) {
	std::ifstream ifs(inputFilePath, std::ios::binary);
	if (!ifs.is_open()) {
		cerr << "Error: Could not open input file " << inputFilePath << " for reading." << endl;
		return nullptr;
	}

	// 阅读文件头信息
	CompressedImageInfo info;
	ifs.read(reinterpret_cast<char*>(&info), sizeof(CompressedImageInfo));

	if (ifs.fail()) {
		cerr << "Error: Failed to read compressed image info." << endl;
		ifs.close();
		return nullptr;
	}

	cout << "Decompression Info:\n";
	cout << "Padded size: " << info.width << " x " << info.height << endl;
	cout << "Original size: " << info.originalWidth << " x " << info.originalHeight << endl;
	cout << "Decomposition levels: " << info.decompositionLevel << endl;
	cout << "Threshold:" << info.threshold << endl;
	cout << "Compressed data size (in bits): " << info.compressedDataSize << endl;

	UINT paddedWidth = info.width;
	UINT paddedHeight = info.height;
	originalWidth = info.originalWidth;
	originalHeight = info.originalHeight;
	int decompositionLevel = info.decompositionLevel;

	// 反序列化Huffman树
	HuffmanNode* huffmanTree = deserialize(ifs);
	if (!huffmanTree) {
		cerr << "Error: Failed to deserialize Huffman tree." << endl;
		ifs.close();
		return nullptr;
	}

	vector<bool> encodedData = readCompressedData(ifs, info.compressedDataSize);
	cout << "Read encoded data size (in bits): " << encodedData.size() << endl;

	vector<int> decodedDigits = decode(encodedData, huffmanTree);
	cout << "Decoded " << decodedDigits.size() << " integers." << endl;

	ifs.close();

	// 转换回RGBA系数
	// 先验证数据完整性
	UINT expectedSize = paddedWidth * paddedHeight * 4;
	if (decodedDigits.size() != expectedSize) {
		cerr << "Error: Decoded data size mismatch! Expected: " << expectedSize
			<< " Actual: " << decodedDigits.size() << endl;
	}
	vector<vector<vector<double>>> rgbCoefficients = IntegersToRGBA(decodedDigits, paddedWidth, paddedHeight);

	// 对每个通道进行逆Haar小波变换
	for (int c = 0; c < 4; c++) {
		cout << "Converting " << c << " channel..." << endl;

		// 读取一个通道的数据
		vector<vector<double>> channelData(paddedHeight, vector<double>(paddedWidth, 0.0));
		for (UINT y = 0; y < paddedHeight; ++y) {
			for (UINT x = 0; x < paddedWidth; ++x) {
				channelData[y][x] = rgbCoefficients[y][x][c];
			}
		}

		// 多层逆Haar变换
		for (int level = 0; level < decompositionLevel; ++level) {
			inverseHaar2D(channelData);
		}

		// 存储变换结果
		for (UINT y = 0; y < paddedHeight; ++y) {
			for (UINT x = 0; x < paddedWidth; ++x) {
				rgbCoefficients[y][x][c] = channelData[y][x];
			}
		}
	}

	cout << "Inverse Haar transform completed." << endl;
	BYTE* decompressedData = postprocessRGBA(rgbCoefficients, originalWidth, originalHeight);

	deleteHuffmanTree(huffmanTree);

	if (decompressedData) {
		cout << "Decompression completed successfully." << endl;
	}

	return decompressedData;
}


// ----------------Haar小波函数实现----------------

// 1D的Haar小波函数，用于后面的2D小波变换
void Compressor::haar1D(vector<double>& data, int len) {
	if (len <= 1) {
		return;
	}

	vector<double> temp(len, 0.0);
	int h = len / 2;
	// 小波变换后的一组，前一个来自和， 后一个来自差
	for (size_t i = 0; i < h; ++i) {
		temp[i] = (data[2 * i] + data[2 * i + 1]) / sqrt(2.0);
		temp[i + h] = (data[2 * i] - data[2 * i + 1]) / sqrt(2.0);
	}
	// 如果是奇数大小，处理最后一个未配对元素
	if (len % 2 == 1) {
		temp[len - 1] = data[len - 1];
	}
	for (size_t i = 0; i < len; ++i) {
		data[i] = temp[i];
	}

	// 将变换结果copy到原数组上面
	for (size_t i = 0; i < len; ++i) {
		data[i] = temp[i];
	}
}

// 1DHaar变换解码
void Compressor::inverseHaar1D(vector<double>& data, int len) {
	if (len <= 1) {
		return;
	}

	vector<double> temp(len, 0.0);
	int h = len / 2;

	for (size_t i = 0; i < h; ++i) {
		temp[2 * i] = (data[i] + data[i + h]) / sqrt(2.0);
		temp[2 * i + 1] = (data[i] - data[i + h]) / sqrt(2.0);
	}

	// 如果是奇数长度，直接保留最后一个元素
	if (len % 2 == 1) {
		temp[len - 1] = data[len - 1];
	}

	for (size_t i = 0; i < len; ++i) {
		data[i] = temp[i];
	}
}

// 2D的Haar小波变换，用于图片各通道数据处理
void Compressor::haar2D(vector<vector<double>>& block) {
	int rows = block.size();
	int cols = block[0].size();

	// 先对row进行一轮1D变换
	for (size_t i = 0; i < rows; ++i) {
		haar1D(block[i], cols);
	}

	// 再对col进行变换，同时存储数据
	for (size_t j = 0; j < cols; ++j) {
		vector<double> col_temp(rows, 0.0);
		for (size_t i = 0; i < rows; ++i) {
			col_temp[i] = block[i][j];
		}
		haar1D(col_temp, rows);
		for (size_t i = 0; i < rows; ++i) {
			block[i][j] = col_temp[i];
		}
	}
}

// 2DHaar小波变换解码
void Compressor::inverseHaar2D(vector<vector<double>>& block) {
	int rows = block.size();
	int cols = block[0].size();

	// 与编码相反的步骤，先对col解码
	for (size_t j = 0; j < cols; ++j) {
		vector<double> col_temp(rows, 0.0);
		for (size_t i = 0; i < rows; ++i) {
			col_temp[i] = block[i][j];
		}
		inverseHaar1D(col_temp, rows);
		for (size_t i = 0; i < rows; ++i) {
			block[i][j] = col_temp[i];
		}
	}

	// 再对row解码
	for (size_t i = 0; i < rows; ++i) {
		inverseHaar1D(block[i], cols);
	}
}


// ----------------色彩处理----------------

// 二维图*4个通道的处理
vector<vector<vector<double>>> Compressor::preprocessRGBA(BYTE* rgbData, UINT originalWidth,
	UINT originalHeight, UINT& paddedWidth, UINT& paddedHeight) {
	// 先做扩展处理，使得像素大小可以为2的幂。便于Haar变换，可以避免不能整除的情况
	paddedWidth = 1;
	while (paddedWidth < originalWidth) paddedWidth <<= 1;
	paddedHeight = 1;
	while (paddedHeight < originalHeight) paddedHeight <<= 1;

	// 一个三维数组[height][width][0-3]分别存放RGBA下各个点的信息
	vector<vector<vector<double>>> rgbArrayData(paddedHeight, vector<vector<double>>(paddedWidth, vector<double>(4, 0.0)));

	// 传入四通道的数值
	for (UINT y = 0; y < originalHeight; ++y) {
		for (UINT x = 0; x < originalWidth; ++x) {
			int index = y * originalWidth * 4 + x * 4;
			rgbArrayData[y][x][0] = static_cast<double>(rgbData[index]);     // R 
			rgbArrayData[y][x][1] = static_cast<double>(rgbData[index + 1]); // G 
			rgbArrayData[y][x][2] = static_cast<double>(rgbData[index + 2]); // B 
			rgbArrayData[y][x][3] = static_cast<double>(rgbData[index + 3]); // A 
		}
	}

	return rgbArrayData;
}

BYTE* Compressor::postprocessRGBA(vector<vector<vector<double>>>& rgbData, UINT originalWidth, UINT originalHeight) {
	BYTE* rgbOutputData = new BYTE[originalWidth * originalHeight * 4];

	for (UINT y = 0; y < originalHeight; ++y) {
		for (UINT x = 0; x < originalWidth; ++x) {
			int index = y * originalWidth * 4 + x * 4;
			// 按照通道与索引对应
			rgbOutputData[index] = static_cast<BYTE>(max(0.0, min(255.0, rgbData[y][x][0])));     // R
			rgbOutputData[index + 1] = static_cast<BYTE>(max(0.0, min(255.0, rgbData[y][x][1]))); // G
			rgbOutputData[index + 2] = static_cast<BYTE>(max(0.0, min(255.0, rgbData[y][x][2]))); // B
			rgbOutputData[index + 3] = static_cast<BYTE>(max(0.0, min(255.0, rgbData[y][x][3]))); // A 
		}
	}
	return rgbOutputData;
}

// 对RGBA量化，过小的数据直接改为0，比较接近的数据化为一致，提高后续压缩效率
void Compressor::quantizeRGBA(vector<vector<vector<double>>>& coefficients, double threshold) {
	for (auto& row : coefficients) {
		for (auto& pixel : row) {
			for (int c = 0; c < 4; ++c) { // 4个通道分别处理
				if (abs(pixel[c]) < threshold) {
					pixel[c] = 0.0; // 小于阈值的全部设为0
				}
				else {
					// 近似比较相近的数据
					pixel[c] = round(pixel[c] / threshold) * threshold;
				}
			}
		}
	}
}

// 化小数为整数
vector<int> Compressor::RGBAToIntegers(vector<vector<vector<double>>>& coefficients) {
	vector<int> integers;

	for (const auto& row : coefficients) {
		for (const auto& pixel : row) {
			for (int c = 0; c < 4; ++c) { 
				// 保留三位小数，再截断
				integers.push_back(static_cast<int>(round(pixel[c] * 1000)));
			}
		}
	}

	return integers;
}

// 整数化回小数
vector<vector<vector<double>>> Compressor::IntegersToRGBA(std::vector<int>& integers, UINT width, UINT height) {
	UINT expectedSize = width * height * 4;
	std::vector<std::vector<std::vector<double>>> coefficients(height,
		std::vector<std::vector<double>>(width, vector<double>(4, 0.0)));

	// 先检查传入的数据是否完整
	if (integers.size() < expectedSize) {
		cerr << "Error: Size unexpected! Expect: " << expectedSize << " Actual: " << integers.size() << endl;
		return coefficients;
	}

	int idx = 0;
	// 按照索引关系转回
	for (UINT y = 0; y < height && idx < (int)integers.size() - 2; ++y) {
		for (UINT x = 0; x < width && idx < (int)integers.size() - 3; ++x) {
			coefficients[y][x][0] = static_cast<double>(integers[idx++]) / 1000.0; // R channel
			coefficients[y][x][1] = static_cast<double>(integers[idx++]) / 1000.0; // G channel
			coefficients[y][x][2] = static_cast<double>(integers[idx++]) / 1000.0; // B channel
			coefficients[y][x][3] = static_cast<double>(integers[idx++]) / 1000.0; // A channel
		}
	}

	return coefficients;
}


// ----------------Huffman树构建，编码，实现----------------

// Huffman树的构建
HuffmanNode* Compressor::buildHuffmanTree(vector<int>& data) {
	// 使用map存储各个数字的频率
	map<int, int> frequency;
	// 遍历数据，统计频率
	for (int val : data) {
		frequency[val]++;
	}

	// 包含节点的优先队列
	priority_queue<HuffmanNode*, vector<HuffmanNode*>, decltype(cmp)> pq(cmp);

	// 入队，并且使得最小频率的排在前面
	for (const auto& pair : frequency) {
		pq.push(new HuffmanNode(pair.second, pair.first));
	}

	// 如果优先队列只有一个元素
	if (pq.size() == 1) {
		HuffmanNode* root = new HuffmanNode(pq.top()->frequency);
		root->left = pq.top();
		return root;
	}

	// 一般情况
	while (pq.size() > 1) {
		HuffmanNode* right = pq.top(); pq.pop();
		HuffmanNode* left = pq.top(); pq.pop();

		HuffmanNode* merged = new HuffmanNode(left->frequency + right->frequency);
		merged->left = left;
		merged->right = right;

		pq.push(merged);
	}

	// 合并到最后队列只剩一个节点，也就是树根
	return pq.top();
}

// 使用递归，由树根向树叶生成Huffman编码方案，保证频率越大，编码越短
void Compressor::generateCodes(HuffmanNode* root, const string& code, map<int, string>& codes) {
	if (!root) return; // 递归终点

	if (root->value != -1) { // 找到叶节点
		codes[root->value] = code.empty() ? "0" : code; // 如果整个树只有一个节点，要人为编码0，否则会出现空串。（虽然很少见）
		return;
	}

	// 向两个子节点递归，左0右1
	generateCodes(root->left, code + "0", codes);
	generateCodes(root->right, code + "1", codes);
}

// 使用bool值编码，因其只占用1bit
vector<bool> Compressor::encode(vector<int>& data, map<int, string>& codes) {
	std::vector<bool> encoded;

	for (int i : data) {
		string& code = codes.at(i);
		for (char bit : code) {
			encoded.push_back(bit == '1');
		}
	}

	return encoded;
}

// 解码
vector<int> Compressor::decode(vector<bool>& encoded, HuffmanNode* root) {
	std::vector<int> decoded;

	if (!root) {
		cerr << "Error: Huffman Tree is empty!" << endl;
		return decoded;
	}

	HuffmanNode* curr = root;

	// 单节点情况（虽然很少见）
	if (root->value != -1) {
		decoded.push_back(curr->value);
		cerr << "One-node Huffman Tree！" << endl;
		return decoded;
	}

	for (size_t i = 0; i < encoded.size(); ++i) {
		bool bit = encoded[i];

		// 左0右1
		if (bit) {
			curr = curr->right;
		}
		else {
			curr = curr->left;
		}

		if (!curr) {
			cerr << "Invalid Huffman Tree!" << endl;
			break;
		}

		if (curr->value != -1) { // 到达叶节点
			decoded.push_back(curr->value);
			curr = root; // 重置，以开始下一轮解码
		}
	}

	return decoded;
}

// 序列化和反序列化，在文件内写入和读取，保证读取文件时可以还原出Huffman树的结构
void Compressor::serialize(HuffmanNode* root, std::ofstream& ofs) {
	// 空节点（一般是错误）标记为0
	if (!root) {
		char marker = 0; // Null node marker
		ofs.write(&marker, 1);
		return;
	}

	// 叶节点
	if (root->value != -1) { 
		char marker = 1;
		ofs.write(&marker, 1);
		ofs.write(reinterpret_cast<const char*>(&root->value), sizeof(int));
	}
	else { 
		char marker = 2;
		ofs.write(&marker, 1);
		serialize(root->left, ofs);  // Serialize left subtree
		serialize(root->right, ofs); // Serialize right subtree
	}
}

HuffmanNode* Compressor::deserialize(ifstream& ifs) {
	char marker;
	ifs.read(&marker, 1);

	if (marker == 0) {
		// 错误检查（可能来自前面编码错误，也有可能是文件损坏）
		cerr << "Wrong or Broken Tree!" << endl;
		return nullptr;
	}
	else if (marker == 1) {
		// 叶节点
		int value;
		ifs.read(reinterpret_cast<char*>(&value), sizeof(int));
		if (!ifs) {
			cerr << "Read error when reading value!" << endl;
			return nullptr;
		}
		return new HuffmanNode(0, value);
	}
	else if (marker == 2) {
		// 内部节点
		HuffmanNode* left = deserialize(ifs);
		HuffmanNode* right = deserialize(ifs);
		HuffmanNode* node = new HuffmanNode(0); // 频率未知，利用默认表示内部节点
		node->left = left;
		node->right = right;
		return node;
	}
	else {
		cerr << "Unknown marker!" << endl;
		return nullptr;
	}
}

// 将bit流写入文件，需要打包为字节传入
void Compressor::writeCompressedData(ofstream& ofs, vector<bool>& data) {
	UINT bitCount = data.size();
	// 先写入压缩数据长度信息
	ofs.write(reinterpret_cast<const char*>(&bitCount), sizeof(UINT));

	for (size_t i = 0; i < data.size(); i += 8) {
		unsigned char byte = 0;
		for (int j = 0; j < 8 && i + j < data.size(); ++j) {
			if (data[i + j]) {
				// 如果data[i+j]位是true，使用位或写入1
				byte |= (1 << (7 - j)); 
			}
		}
		//写入这个字节
		ofs.write(reinterpret_cast<const char*>(&byte), 1);
	}
}

// 读取文件的二进制流
vector<bool> Compressor::readCompressedData(ifstream& ifs, UINT dataSize) {
	UINT bitCount = 0;
	ifs.read(reinterpret_cast<char*>(&bitCount), sizeof(UINT));

	std::vector<bool> data;
	data.reserve(bitCount); // 小优化，比直接添加可以快一点点

	UINT byteCount = (bitCount + 7) / 8;
	for (UINT i = 0; i < byteCount; ++i) {
		BYTE byte;
		ifs.read(reinterpret_cast<char*>(&byte), 1);

		// 按位与取出每个位
		for (int j = 0; j < 8 && data.size() < bitCount; ++j) {
			data.push_back((byte & (1 << (7 - j))) != 0);
		}
	}

	return data;
}

// 内存释放
void Compressor::deleteHuffmanTree(HuffmanNode* root) {
	if (root) {
		deleteHuffmanTree(root->left);  
		deleteHuffmanTree(root->right); 
		delete root;
	}
}