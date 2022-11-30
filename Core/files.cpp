#include "core/files.h"

namespace core
{
	size_t fileSize(std::ifstream& file)
	{
		std::streampos oldPos = file.tellg();
		file.seekg(0, std::ios::beg);
		std::streampos beg = file.tellg();
		file.seekg(0, std::ios::end);
		std::streampos end = file.tellg();
		file.seekg(oldPos, std::ios::beg);
		return static_cast<size_t>(end - beg);
	}
}


bool core::loadBundle(const std::wstring& filepath, std::vector<char>& data)
{
	std::ifstream fin(filepath, std::ios::binary);
	if (false == fin.good())
	{
		fin.close();
		return false;
	}
	size_t size = fileSize(fin);
	if (0 == size)
	{
		fin.close();
		return false;
	}
	data.resize(size);
	fin.read(reinterpret_cast<char*>(&data[0]), size);

	fin.close();
	return true;
}
