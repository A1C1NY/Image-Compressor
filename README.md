# Image Compressor
### 高级语言程序设计（进阶）作业二
## 简介
本项目使用C++编写，实现了基于Haar小波变换和Huffman编码的图像压缩算法。可以接受tiff格式的RGBA彩色图像，并输出压缩后的二进制文件。预计压缩率在23%-35%之间，具体取决于图像内容。
## 功能
- **图像读取**：支持读取tiff格式的RGBA彩色图像。
- **Haar小波变换**：对图像进行Haar小波变换以提取重要特征。
- **量化**：对小波系数进行量化，约去不重要的信息。
- **Huffman编码**：对量化后的数据进行Huffman编码以实现压缩。
- **图像重建**：支持从压缩文件中解码并重建原始图像。并且导出为png格式（虽说比原图大）
## 使用方法
在管理员权限下进入cmd或PowerShell，运行程序。  
使用以下命令进行压缩和解压缩：
```bash
# 压缩图像
Compressor.exe -compress filename.tiff
# 解压缩图像
Compressor.exe -read filename.dat
```
## 依赖
- C++17标准
- 注意使用GBK编码来打开所有中文内容

## 项目文件结构
```
Z_3/
├── include/
│   ├── Compressor.h   // 声明图像压缩/解压缩类及核心算法
│   └── PicReader.h    // 声明BMP图像文件读取类
│
└── src/
    ├── main.cpp       // 程序主入口，处理命令行参数和调用压缩/解压缩流程
    ├── Compressor.cpp // 实现Compressor类，包括小波变换、量化、霍夫曼编码等
    └── PicReader.cpp  // 实现PicReader类，用于解析BMP文件头和像素数据
```

## 贡献
欢迎任何形式的贡献！如果您有改进建议或发现问题，请提交issue或pull request。
