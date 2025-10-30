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
�������� ������Bug

��ʵʹ���ַ���������Ч��ͼ��ѹ������  
*/  

// ѹ����������
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

	// ��ÿ��ͨ������HaarС���任
	for (int c = 0; c < 4; ++c) {
		cout << "Processing channel..." << endl;

		// ��ȡһ��ͨ��������
		vector<vector<double>> channelData(paddedHeight, vector<double>(paddedWidth, 0.0));
		for (UINT y = 0; y < paddedHeight; ++y) {
			for (UINT x = 0; x < paddedWidth; ++x) {
				channelData[y][x] = rgbData[y][x][c];
			}
		}
		// ���Haar�任
		for (int level = 0; level < decompositionLevel; ++level) {
			haar2D(channelData);
		}

		// �仯������ݴ���rgbData
		for (UINT y = 0; y < paddedHeight; ++y) {
			for (UINT x = 0; x < paddedWidth; ++x) {
				rgbData[y][x][c] = channelData[y][x];
			}
		}
	} 

	// ������������������������
	quantizeRGBA(rgbData, threshold);

	// �Ŵ�Ϊ����
	vector<int> integerCoefficients = RGBAToIntegers(rgbData);

	// ����Huffman��
	HuffmanNode* huffmanTree = buildHuffmanTree(integerCoefficients);
	map<int, std::string> huffmanCodes;
	generateCodes(huffmanTree, "", huffmanCodes);
	cout << "Generated " << huffmanCodes.size() << " Huffman codes." << endl;

	// ����
	vector<bool> encodedData = encode(integerCoefficients, huffmanCodes);
	cout << "Encoded data size (in bits): " << encodedData.size() << endl;

	// д���ļ�
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

// ��ѹ����������
BYTE* Compressor::decompressImage(string& inputFilePath, UINT& originalWidth, UINT& originalHeight) {
	std::ifstream ifs(inputFilePath, std::ios::binary);
	if (!ifs.is_open()) {
		cerr << "Error: Could not open input file " << inputFilePath << " for reading." << endl;
		return nullptr;
	}

	// �Ķ��ļ�ͷ��Ϣ
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

	// �����л�Huffman��
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

	// ת����RGBAϵ��
	// ����֤����������
	UINT expectedSize = paddedWidth * paddedHeight * 4;
	if (decodedDigits.size() != expectedSize) {
		cerr << "Error: Decoded data size mismatch! Expected: " << expectedSize
			<< " Actual: " << decodedDigits.size() << endl;
	}
	vector<vector<vector<double>>> rgbCoefficients = IntegersToRGBA(decodedDigits, paddedWidth, paddedHeight);

	// ��ÿ��ͨ��������HaarС���任
	for (int c = 0; c < 4; c++) {
		cout << "Converting " << c << " channel..." << endl;

		// ��ȡһ��ͨ��������
		vector<vector<double>> channelData(paddedHeight, vector<double>(paddedWidth, 0.0));
		for (UINT y = 0; y < paddedHeight; ++y) {
			for (UINT x = 0; x < paddedWidth; ++x) {
				channelData[y][x] = rgbCoefficients[y][x][c];
			}
		}

		// �����Haar�任
		for (int level = 0; level < decompositionLevel; ++level) {
			inverseHaar2D(channelData);
		}

		// �洢�任���
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


// ----------------HaarС������ʵ��----------------

// 1D��HaarС�����������ں����2DС���任
void Compressor::haar1D(vector<double>& data, int len) {
	if (len <= 1) {
		return;
	}

	vector<double> temp(len, 0.0);
	int h = len / 2;
	// С���任���һ�飬ǰһ�����Ժͣ� ��һ�����Բ�
	for (size_t i = 0; i < h; ++i) {
		temp[i] = (data[2 * i] + data[2 * i + 1]) / sqrt(2.0);
		temp[i + h] = (data[2 * i] - data[2 * i + 1]) / sqrt(2.0);
	}
	// �����������С���������һ��δ���Ԫ��
	if (len % 2 == 1) {
		temp[len - 1] = data[len - 1];
	}
	for (size_t i = 0; i < len; ++i) {
		data[i] = temp[i];
	}

	// ���任���copy��ԭ��������
	for (size_t i = 0; i < len; ++i) {
		data[i] = temp[i];
	}
}

// 1DHaar�任����
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

	// ������������ȣ�ֱ�ӱ������һ��Ԫ��
	if (len % 2 == 1) {
		temp[len - 1] = data[len - 1];
	}

	for (size_t i = 0; i < len; ++i) {
		data[i] = temp[i];
	}
}

// 2D��HaarС���任������ͼƬ��ͨ�����ݴ���
void Compressor::haar2D(vector<vector<double>>& block) {
	int rows = block.size();
	int cols = block[0].size();

	// �ȶ�row����һ��1D�任
	for (size_t i = 0; i < rows; ++i) {
		haar1D(block[i], cols);
	}

	// �ٶ�col���б任��ͬʱ�洢����
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

// 2DHaarС���任����
void Compressor::inverseHaar2D(vector<vector<double>>& block) {
	int rows = block.size();
	int cols = block[0].size();

	// ������෴�Ĳ��裬�ȶ�col����
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

	// �ٶ�row����
	for (size_t i = 0; i < rows; ++i) {
		inverseHaar1D(block[i], cols);
	}
}


// ----------------ɫ�ʴ���----------------

// ��άͼ*4��ͨ���Ĵ���
vector<vector<vector<double>>> Compressor::preprocessRGBA(BYTE* rgbData, UINT originalWidth,
	UINT originalHeight, UINT& paddedWidth, UINT& paddedHeight) {
	// ������չ����ʹ�����ش�С����Ϊ2���ݡ�����Haar�任�����Ա��ⲻ�����������
	paddedWidth = 1;
	while (paddedWidth < originalWidth) paddedWidth <<= 1;
	paddedHeight = 1;
	while (paddedHeight < originalHeight) paddedHeight <<= 1;

	// һ����ά����[height][width][0-3]�ֱ���RGBA�¸��������Ϣ
	vector<vector<vector<double>>> rgbArrayData(paddedHeight, vector<vector<double>>(paddedWidth, vector<double>(4, 0.0)));

	// ������ͨ������ֵ
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
			// ����ͨ����������Ӧ
			rgbOutputData[index] = static_cast<BYTE>(max(0.0, min(255.0, rgbData[y][x][0])));     // R
			rgbOutputData[index + 1] = static_cast<BYTE>(max(0.0, min(255.0, rgbData[y][x][1]))); // G
			rgbOutputData[index + 2] = static_cast<BYTE>(max(0.0, min(255.0, rgbData[y][x][2]))); // B
			rgbOutputData[index + 3] = static_cast<BYTE>(max(0.0, min(255.0, rgbData[y][x][3]))); // A 
		}
	}
	return rgbOutputData;
}

// ��RGBA��������С������ֱ�Ӹ�Ϊ0���ȽϽӽ������ݻ�Ϊһ�£���ߺ���ѹ��Ч��
void Compressor::quantizeRGBA(vector<vector<vector<double>>>& coefficients, double threshold) {
	for (auto& row : coefficients) {
		for (auto& pixel : row) {
			for (int c = 0; c < 4; ++c) { // 4��ͨ���ֱ���
				if (abs(pixel[c]) < threshold) {
					pixel[c] = 0.0; // С����ֵ��ȫ����Ϊ0
				}
				else {
					// ���ƱȽ����������
					pixel[c] = round(pixel[c] / threshold) * threshold;
				}
			}
		}
	}
}

// ��С��Ϊ����
vector<int> Compressor::RGBAToIntegers(vector<vector<vector<double>>>& coefficients) {
	vector<int> integers;

	for (const auto& row : coefficients) {
		for (const auto& pixel : row) {
			for (int c = 0; c < 4; ++c) { 
				// ������λС�����ٽض�
				integers.push_back(static_cast<int>(round(pixel[c] * 1000)));
			}
		}
	}

	return integers;
}

// ��������С��
vector<vector<vector<double>>> Compressor::IntegersToRGBA(std::vector<int>& integers, UINT width, UINT height) {
	UINT expectedSize = width * height * 4;
	std::vector<std::vector<std::vector<double>>> coefficients(height,
		std::vector<std::vector<double>>(width, vector<double>(4, 0.0)));

	// �ȼ�鴫��������Ƿ�����
	if (integers.size() < expectedSize) {
		cerr << "Error: Size unexpected! Expect: " << expectedSize << " Actual: " << integers.size() << endl;
		return coefficients;
	}

	int idx = 0;
	// ����������ϵת��
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


// ----------------Huffman�����������룬ʵ��----------------

// Huffman���Ĺ���
HuffmanNode* Compressor::buildHuffmanTree(vector<int>& data) {
	// ʹ��map�洢�������ֵ�Ƶ��
	map<int, int> frequency;
	// �������ݣ�ͳ��Ƶ��
	for (int val : data) {
		frequency[val]++;
	}

	// �����ڵ�����ȶ���
	priority_queue<HuffmanNode*, vector<HuffmanNode*>, decltype(cmp)> pq(cmp);

	// ��ӣ�����ʹ����СƵ�ʵ�����ǰ��
	for (const auto& pair : frequency) {
		pq.push(new HuffmanNode(pair.second, pair.first));
	}

	// ������ȶ���ֻ��һ��Ԫ��
	if (pq.size() == 1) {
		HuffmanNode* root = new HuffmanNode(pq.top()->frequency);
		root->left = pq.top();
		return root;
	}

	// һ�����
	while (pq.size() > 1) {
		HuffmanNode* right = pq.top(); pq.pop();
		HuffmanNode* left = pq.top(); pq.pop();

		HuffmanNode* merged = new HuffmanNode(left->frequency + right->frequency);
		merged->left = left;
		merged->right = right;

		pq.push(merged);
	}

	// �ϲ���������ֻʣһ���ڵ㣬Ҳ��������
	return pq.top();
}

// ʹ�õݹ飬����������Ҷ����Huffman���뷽������֤Ƶ��Խ�󣬱���Խ��
void Compressor::generateCodes(HuffmanNode* root, const string& code, map<int, string>& codes) {
	if (!root) return; // �ݹ��յ�

	if (root->value != -1) { // �ҵ�Ҷ�ڵ�
		codes[root->value] = code.empty() ? "0" : code; // ���������ֻ��һ���ڵ㣬Ҫ��Ϊ����0���������ֿմ�������Ȼ���ټ���
		return;
	}

	// �������ӽڵ�ݹ飬��0��1
	generateCodes(root->left, code + "0", codes);
	generateCodes(root->right, code + "1", codes);
}

// ʹ��boolֵ���룬����ֻռ��1bit
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

// ����
vector<int> Compressor::decode(vector<bool>& encoded, HuffmanNode* root) {
	std::vector<int> decoded;

	if (!root) {
		cerr << "Error: Huffman Tree is empty!" << endl;
		return decoded;
	}

	HuffmanNode* curr = root;

	// ���ڵ��������Ȼ���ټ���
	if (root->value != -1) {
		decoded.push_back(curr->value);
		cerr << "One-node Huffman Tree��" << endl;
		return decoded;
	}

	for (size_t i = 0; i < encoded.size(); ++i) {
		bool bit = encoded[i];

		// ��0��1
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

		if (curr->value != -1) { // ����Ҷ�ڵ�
			decoded.push_back(curr->value);
			curr = root; // ���ã��Կ�ʼ��һ�ֽ���
		}
	}

	return decoded;
}

// ���л��ͷ����л������ļ���д��Ͷ�ȡ����֤��ȡ�ļ�ʱ���Ի�ԭ��Huffman���Ľṹ
void Compressor::serialize(HuffmanNode* root, std::ofstream& ofs) {
	// �սڵ㣨һ���Ǵ��󣩱��Ϊ0
	if (!root) {
		char marker = 0; // Null node marker
		ofs.write(&marker, 1);
		return;
	}

	// Ҷ�ڵ�
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
		// �����飨��������ǰ��������Ҳ�п������ļ��𻵣�
		cerr << "Wrong or Broken Tree!" << endl;
		return nullptr;
	}
	else if (marker == 1) {
		// Ҷ�ڵ�
		int value;
		ifs.read(reinterpret_cast<char*>(&value), sizeof(int));
		if (!ifs) {
			cerr << "Read error when reading value!" << endl;
			return nullptr;
		}
		return new HuffmanNode(0, value);
	}
	else if (marker == 2) {
		// �ڲ��ڵ�
		HuffmanNode* left = deserialize(ifs);
		HuffmanNode* right = deserialize(ifs);
		HuffmanNode* node = new HuffmanNode(0); // Ƶ��δ֪������Ĭ�ϱ�ʾ�ڲ��ڵ�
		node->left = left;
		node->right = right;
		return node;
	}
	else {
		cerr << "Unknown marker!" << endl;
		return nullptr;
	}
}

// ��bit��д���ļ�����Ҫ���Ϊ�ֽڴ���
void Compressor::writeCompressedData(ofstream& ofs, vector<bool>& data) {
	UINT bitCount = data.size();
	// ��д��ѹ�����ݳ�����Ϣ
	ofs.write(reinterpret_cast<const char*>(&bitCount), sizeof(UINT));

	for (size_t i = 0; i < data.size(); i += 8) {
		unsigned char byte = 0;
		for (int j = 0; j < 8 && i + j < data.size(); ++j) {
			if (data[i + j]) {
				// ���data[i+j]λ��true��ʹ��λ��д��1
				byte |= (1 << (7 - j)); 
			}
		}
		//д������ֽ�
		ofs.write(reinterpret_cast<const char*>(&byte), 1);
	}
}

// ��ȡ�ļ��Ķ�������
vector<bool> Compressor::readCompressedData(ifstream& ifs, UINT dataSize) {
	UINT bitCount = 0;
	ifs.read(reinterpret_cast<char*>(&bitCount), sizeof(UINT));

	std::vector<bool> data;
	data.reserve(bitCount); // С�Ż�����ֱ����ӿ��Կ�һ���

	UINT byteCount = (bitCount + 7) / 8;
	for (UINT i = 0; i < byteCount; ++i) {
		BYTE byte;
		ifs.read(reinterpret_cast<char*>(&byte), 1);

		// ��λ��ȡ��ÿ��λ
		for (int j = 0; j < 8 && data.size() < bitCount; ++j) {
			data.push_back((byte & (1 << (7 - j))) != 0);
		}
	}

	return data;
}

// �ڴ��ͷ�
void Compressor::deleteHuffmanTree(HuffmanNode* root) {
	if (root) {
		deleteHuffmanTree(root->left);  
		deleteHuffmanTree(root->right); 
		delete root;
	}
}