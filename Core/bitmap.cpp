/*
tdogl::Bitmap

Copyright 2012 Thomas Dalling - http://tomdalling.com/

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "Common/Bitmap.h"
#include <stdexcept>
#include "win/win32.h"
//uses stb_image to try load files
#define STBI_FAILURE_USERMSG
#include "Common/stbimage.h"



inline unsigned char AverageRGB(unsigned char rgb[3]) {
	return (unsigned char)(((double)rgb[0] + (double)rgb[1] + (double)rgb[2]) / 3.0);
}

static void Grayscale2GrayscaleAlpha(unsigned char* src, unsigned char* dest) {
	dest[0] = src[0];
	dest[1] = 255;
}

static void Grayscale2RGB(unsigned char* src, unsigned char* dest) {
	dest[0] = src[0];
	dest[1] = src[0];
	dest[2] = src[0];
}

static void Grayscale2RGBA(unsigned char* src, unsigned char* dest) {
	dest[0] = src[0];
	dest[1] = src[0];
	dest[2] = src[0];
	dest[3] = 255;
}

static void GrayscaleAlpha2Grayscale(unsigned char* src, unsigned char* dest) {
	dest[0] = src[0];
}

static void GrayscaleAlpha2RGB(unsigned char* src, unsigned char* dest) {
	dest[0] = src[0];
	dest[1] = src[0];
	dest[2] = src[0];
}

static void GrayscaleAlpha2RGBA(unsigned char* src, unsigned char* dest) {
	dest[0] = src[0];
	dest[1] = src[0];
	dest[2] = src[0];
	dest[3] = src[1];
}

static void RGB2Grayscale(unsigned char* src, unsigned char* dest) {
	dest[0] = AverageRGB(src);
}

static void RGB2GrayscaleAlpha(unsigned char* src, unsigned char* dest) {
	dest[0] = AverageRGB(src);
	dest[1] = 255;
}

static void RGB2RGBA(unsigned char* src, unsigned char* dest) {
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
	dest[3] = 255;
}

static void RGBA2Grayscale(unsigned char* src, unsigned char* dest) {
	dest[0] = AverageRGB(src);
}

static void RGBA2GrayscaleAlpha(unsigned char* src, unsigned char* dest) {
	dest[0] = AverageRGB(src);
	dest[1] = src[3];
}

static void RGBA2RGB(unsigned char* src, unsigned char* dest) {
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
}

typedef void(*FormatConverterFunc)(unsigned char*, unsigned char*);

static FormatConverterFunc ConverterFuncForFormats(Bitmap::Format srcFormat, Bitmap::Format destFormat) {
	if (srcFormat == destFormat)
		throw std::runtime_error("Just use memcpy if pixel formats are the same");

	switch (srcFormat) {

	case Bitmap::Format_Grayscale:
		switch (destFormat) {
		case Bitmap::Format_GrayscaleAlpha: return Grayscale2GrayscaleAlpha;
		case Bitmap::Format_RGB:            return Grayscale2RGB;
		case Bitmap::Format_RGBA:           return Grayscale2RGBA;
		default:
			throw std::runtime_error("Unhandled bitmap format");
		}
		break;

	case Bitmap::Format_GrayscaleAlpha:
		switch (destFormat) {
		case Bitmap::Format_Grayscale: return GrayscaleAlpha2Grayscale;
		case Bitmap::Format_RGB:       return GrayscaleAlpha2RGB;
		case Bitmap::Format_RGBA:      return GrayscaleAlpha2RGBA;
		default:
			throw std::runtime_error("Unhandled bitmap format");
		}
		break;

	case Bitmap::Format_RGB:
		switch (destFormat) {
		case Bitmap::Format_Grayscale:      return RGB2Grayscale;
		case Bitmap::Format_GrayscaleAlpha: return RGB2GrayscaleAlpha;
		case Bitmap::Format_RGBA:           return RGB2RGBA;
		default:
			throw std::runtime_error("Unhandled bitmap format");
		}
		break;

	case Bitmap::Format_RGBA:
		switch (destFormat) {
		case Bitmap::Format_Grayscale:      return RGBA2Grayscale;
		case Bitmap::Format_GrayscaleAlpha: return RGBA2GrayscaleAlpha;
		case Bitmap::Format_RGB:            return RGBA2RGB;
		default:
			throw std::runtime_error("Unhandled bitmap format");
		}
		break;

	default:
		throw std::runtime_error("Unhandled bitmap format");
	}
}


/*
* Misc funcs
*/

inline unsigned GetPixelOffset(unsigned col, unsigned row, unsigned width, unsigned height, Bitmap::Format format) {
	return (row*width + col)*format;
}

inline bool RectsOverlap(unsigned srcCol, unsigned srcRow, unsigned destCol, unsigned destRow, unsigned width, unsigned height) {
	unsigned colDiff = srcCol > destCol ? srcCol - destCol : destCol - srcCol;
	if (colDiff < width)
		return true;

	unsigned rowDiff = srcRow > destRow ? srcRow - destRow : destRow - srcRow;
	if (rowDiff < height)
		return true;

	return false;
}


/*
* Bitmap class
*/

Bitmap::Bitmap(unsigned width,
	unsigned height,
	Format format,
	const unsigned char* pixels) :
	_pixels(NULL)
{
	_set(width, height, format, pixels);
}

Bitmap::~Bitmap() {
	if (_pixels) free(_pixels);
}

std::shared_ptr<Bitmap> Bitmap::bitmapFromFile(const std::wstring& filePath) {
	int width, height, channels;
	unsigned char* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, 4);
	if (!pixels)
	{
		return nullptr;
	}

	std::shared_ptr<Bitmap> bmp = std::make_shared<Bitmap>(width, height, (Format)channels, pixels);
	stbi_image_free(pixels);
	return bmp;
}

Bitmap Bitmap::bitmapFromMemory(const uint8_t* buffer, int len)
{
	int width, height, channels;
	unsigned char* pixels = stbi_load_from_memory(buffer, len, &width, &height, &channels, 4);
	if (!pixels) throw std::runtime_error(stbi_failure_reason());

	Bitmap bmp(width, height, (Format)channels, pixels);
	stbi_image_free(pixels);
	return bmp;
}

Bitmap::Bitmap(const Bitmap& other) :
	_pixels(NULL)
{
	_set(other._width, other._height, other._format, other._pixels);
}

Bitmap::Bitmap(Bitmap&& other)
{
    _pixels = other._pixels;
    _width = other._width;
    _height = other._height;
    _format = other._format;

    other._pixels = nullptr;
    other._width = 0;
    other._height = 0;
}

Bitmap& Bitmap::operator = (const Bitmap& other) {
	_set(other._width, other._height, other._format, other._pixels);
	return *this;
}

Bitmap& Bitmap::operator=(Bitmap&& other)
{
    if (this != &other)
    {
        _pixels = other._pixels;
        _width = other._width;
        _height = other._height;
        _format = other._format;

        other._pixels = nullptr;
        other._width = 0;
        other._height = 0;
    }
    return *this;
}

unsigned int Bitmap::width() const {
	return _width;
}

unsigned int Bitmap::height() const {
	return _height;
}

Bitmap::Format Bitmap::format() const {
	return _format;
}

unsigned char* Bitmap::pixelBuffer() const {
	return _pixels;
}

unsigned char* Bitmap::getPixel(unsigned int column, unsigned int row) const {
	if (column >= _width || row >= _height)
		throw std::runtime_error("Pixel coordinate out of bounds");

	return _pixels + GetPixelOffset(column, row, _width, _height, _format);
}

void Bitmap::setPixel(unsigned int column, unsigned int row, const unsigned char* pixel) {
	unsigned char* myPixel = getPixel(column, row);
	memcpy(myPixel, pixel, _format);
}

void Bitmap::flipVertically() {
	unsigned long rowSize = _format*_width;
	unsigned char* rowBuffer = new unsigned char[rowSize];
	unsigned halfRows = _height / 2;

	for (unsigned rowIdx = 0; rowIdx < halfRows; ++rowIdx) {
		unsigned char* row = _pixels + GetPixelOffset(0, rowIdx, _width, _height, _format);
		unsigned char* oppositeRow = _pixels + GetPixelOffset(0, _height - rowIdx - 1, _width, _height, _format);

		memcpy(rowBuffer, row, rowSize);
		memcpy(row, oppositeRow, rowSize);
		memcpy(oppositeRow, rowBuffer, rowSize);
	}

	delete rowBuffer;
}

void Bitmap::rotate90CounterClockwise() {
	unsigned char* newPixels = (unsigned char*)malloc(_format*_width*_height);

	for (unsigned row = 0; row < _height; ++row) {
		for (unsigned col = 0; col < _width; ++col) {
			unsigned srcOffset = GetPixelOffset(col, row, _width, _height, _format);
			unsigned destOffset = GetPixelOffset(row, _width - col - 1, _height, _width, _format);
			memcpy(newPixels + destOffset, _pixels + srcOffset, _format); //copy one pixel
		}
	}

	free(_pixels);
	_pixels = newPixels;

	unsigned swapTmp = _height;
	_height = _width;
	_width = swapTmp;
}

void Bitmap::copyRectFromBitmap(const Bitmap& src,
	unsigned srcCol,
	unsigned srcRow,
	unsigned destCol,
	unsigned destRow,
	unsigned width,
	unsigned height)
{
	if (srcCol == 0 && srcRow == 0 && width == 0 && height == 0) {
		width = src.width();
		height = src.height();
	}

	if (width == 0 || height == 0)
		throw std::runtime_error("Can't copy zero height/width rectangle");

	if (srcCol + width > src.width() || srcRow + height > src.height())
		throw std::runtime_error("Rectangle doesn't fit within source bitmap");

	if (destCol + width > _width || destRow + height > _height)
		throw std::runtime_error("Rectangle doesn't fit within destination bitmap");

	if (_pixels == src._pixels && RectsOverlap(srcCol, srcRow, destCol, destRow, width, height))
		throw std::runtime_error("Source and destination are the same bitmap, and rects overlap. Not allowed!");

	FormatConverterFunc converter = NULL;
	if (_format != src._format)
		converter = ConverterFuncForFormats(_format, src._format);

	for (unsigned row = 0; row < height; ++row) {
		for (unsigned col = 0; col < width; ++col) {
			unsigned char* srcPixel = src._pixels + GetPixelOffset(srcCol + col, srcRow + row, src._width, src._height, src._format);
			unsigned char* destPixel = _pixels + GetPixelOffset(destCol + col, destRow + row, _width, _height, _format);

			if (converter) {
				converter(srcPixel, destPixel);
			}
			else {
				memcpy(destPixel, srcPixel, _format);
			}
		}
	}
}

void Bitmap::_set(unsigned width,
	unsigned height,
	Format format,
	const unsigned char* pixels)
{
	if (width == 0) throw std::runtime_error("Zero width bitmap");
	if (height == 0) throw std::runtime_error("Zero height bitmap");
	if (format <= 0 || format > 4) throw std::runtime_error("Invalid bitmap format");

	_width = width;
	_height = height;
	_format = format;

	size_t newSize = _width * _height * _format;
	if (_pixels) {
		_pixels = (unsigned char*)realloc(_pixels, newSize);
	}
	else {
		_pixels = (unsigned char*)malloc(newSize);
	}

	if (pixels)
		memcpy(_pixels, pixels, newSize);
}


unsigned char* Bitmap::getLinePixel(unsigned int row) const
{
	if (row >= _height)
		throw std::runtime_error("Pixel coordinate out of bounds");

	return _pixels + GetPixelOffset(0, row, _width, _height, _format);
}

Bitmap Bitmap::scale(unsigned int width, unsigned int height)
{
	Bitmap routput(width, height, _format);


	float width_scale = (float)_width / width;     // 列缩放比例，相对于算法前面讲的k1
	float height_scale = (float)_height / height;   // 行缩放比例，即k2

	// 2, 采样
	for (uint32_t i = 0; i < height; i++)  // 注意i,j的范围, i < height * img.rows / height;
	{
		for (uint32_t j = 0; j < width; j++)
		{
			routput.setPixel(j, i, getPixel(uint32_t(j * width_scale), uint32_t(i * height_scale)));
		}
	}

	return routput;
}

uint8_t get_scale_value(Bitmap& input_img, float raw_i, float raw_j)
{
	uint32_t i = uint32_t(raw_i);
	uint32_t j = uint32_t(raw_j);
	float u = raw_i - i;
	float v = raw_j - j;

	//注意处理边界问题，容易越界
	if (i + 1 >= input_img.height() || j + 1 >= input_img.width())
	{
		uint8_t* p = input_img.getLinePixel(i);
		return p[j];
	}

	uint8_t* p = input_img.getLinePixel(i);
	uint8_t x1 = p[j];  //f(i,j)
	uint8_t x2 = p[j + 1];  //f(i,j+1)
	p = input_img.getLinePixel(i + 1);
	uint8_t x3 = p[j];   //(i+1,j)
	uint8_t x4 = p[j + 1];  //f(i+1,j+1) 

   // printf("%d %d\n", i, j);
	return uint8_t((1 - u)*(1 - v)*x1 + (1 - u)*v*x2 + u * (1 - v)*x3 + u * v*x4);
}

inline uint8_t getRValue(unsigned char* color)
{
	return color[0];
}

inline uint8_t getGValue(unsigned char* color)
{
	return color[1];
}

inline uint8_t getBValue(unsigned char* color)
{
	return color[2];
}

inline uint8_t getAValue(unsigned char* color)
{
	return color[3];
}

Bitmap Bitmap::linear_scale(unsigned int width, unsigned int height)
{
	Bitmap routput(width, height, _format);

	int w0 = _width;
	int h0 = _height;
	int pitch0 = _width * _format;

	int w1 = width;
	int h1 = height;
	int pitch1 = width * _format;

	float fw = float(w0) / w1;
	float fh = float(h0) / h1;

	int y1, y2, x1, x2, x0, y0;
	float fx1, fx2, fy1, fy2;
	for (int y = 0; y < h1; y++)
	{
		y0 = y * int(fh);
		y1 = int(y0);
		if (y1 == h0 - 1)    y2 = y1;
		else y2 = y1 + 1;

		fy1 = float(y1 - y0);
		fy2 = 1.0f - fy1;
		for (int x = 0; x < w1; x++)
		{
			x0 = x * int(fw);
			x1 = int(x0);
			if (x1 == w0 - 1)    x2 = x1;
			else x2 = x1 + 1;

			fx1 = float(y1 - y0);
			fx2 = 1.0f - fx1;

			float s1 = fx1 * fy1;
			float s2 = fx2 * fy1;
			float s3 = fx2 * fy2;
			float s4 = fx1 * fy2;

			unsigned char* c1, *c2, *c3, *c4;
			c1 = getPixel(x1, y1);
			c2 = getPixel(x2, y1);
			c3 = getPixel(x1, y2);
			c4 = getPixel(x2, y2);
			uint8_t color[4];
			color[0] = (uint8_t)(getRValue(c1)*s3) + (uint8_t)(getRValue(c2)*s4) + (uint8_t)(getRValue(c3)*s2) + (uint8_t)(getRValue(c4)*s1);
			color[1] = (uint8_t)(getGValue(c1)*s3) + (uint8_t)(getGValue(c2)*s4) + (uint8_t)(getGValue(c3)*s2) + (uint8_t)(getGValue(c4)*s1);
			color[2] = (uint8_t)(getBValue(c1)*s3) + (uint8_t)(getBValue(c2)*s4) + (uint8_t)(getBValue(c3)*s2) + (uint8_t)(getBValue(c4)*s1);
			color[3] = (uint8_t)(getAValue(c1)*s3) + (uint8_t)(getAValue(c2)*s4) + (uint8_t)(getAValue(c3)*s2) + (uint8_t)(getAValue(c4)*s1);
			routput.setPixel(x, y, color);
		}
	}
	return routput;
}


void Bitmap::SaveBitmap(const wchar_t* fileName)
{
	HANDLE hFile = ::CreateFile(fileName,            // file to create 
		GENERIC_WRITE,                // open for writing 
		0,                            // do not share 
		NULL,                         // default security 
		OPEN_ALWAYS,                  // overwrite existing 
		FILE_ATTRIBUTE_NORMAL,        // normal file 
		NULL);                        // no attr. template 
	if (!hFile || hFile == INVALID_HANDLE_VALUE)
	{
		return;	// 
	}

	DWORD dwSizeBytes = _width*_height*_format;

	// fill in the headers
	BITMAPFILEHEADER bmfh;
	bmfh.bfType = 0x4D42; // 'BM'
	bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwSizeBytes;//整个文件的大小
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);//图像数据偏移量，即图像数据在文件中的保存位置

	DWORD dwBytesWritten;
	::WriteFile(hFile, &bmfh, sizeof(bmfh), &dwBytesWritten, NULL);
	if (dwBytesWritten != sizeof(bmfh))
	{
	}

	BITMAPINFOHEADER bmih;

	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biWidth = _width;
	bmih.biHeight = _height;
	bmih.biPlanes = 1; // 图像的目标显示设备的位数，通常为1
	bmih.biBitCount = _format * 8; 
	bmih.biCompression = BI_RGB;// 是否压缩
	bmih.biSizeImage = 0;//图像大小的字节数
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;

	::WriteFile(hFile, &bmih, sizeof(bmih), &dwBytesWritten, NULL);
	if (dwBytesWritten != sizeof(bmih))
	{
	}

	::WriteFile(hFile, this->pixelBuffer(), dwSizeBytes, &dwBytesWritten, NULL);
	if (dwBytesWritten != dwSizeBytes)
	{
	}

	::CloseHandle(hFile);
}


