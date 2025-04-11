#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>

// 检查是否为 GB2312 双字节字符的首字节
static int is_gb2312_lead_byte(unsigned char c) {
	return (c >= 0xA1 && c <= 0xFE);
}

// 跳过空白字符（支持 GB2312 的 ASCII 空白）
static const char* skip_whitespace(const char* p) {
	while (isspace((unsigned char)*p)) p++;
	return p;
}

// 解析字段宽度或精度
static int parse_number(const char** f) {
	int num = 0;
	while (isdigit(**f)) {
		num = num * 10 + (**f - '0');
		(*f)++;
	}
	return num;
}

// 抛出异常提醒函数
static void throw_format_error(const char* input, const char* format, const char* p, const char* f) {
	fprintf(stderr, "Format mismatch error:\n");
	fprintf(stderr, "  Input: %s\n", input);
	fprintf(stderr, "  Format: %s\n", format);
	fprintf(stderr, "  Position: input[%ld]='%c', format[%ld]='%c'\n",
		(long)(p - input), *p ? *p : '\0', (long)(f - format), *f);
}

// 支持 GB2312 和扩展格式的 my_sscanf 函数，无需 sizeof(str)
int my_vsscanf(const char* input, const char* format, va_list args) {
	int count = 0;          // 成功解析的参数计数
	const char* p = input;  // 输入字符串指针
	const char* f = format; // 格式字符串指针
	bool hasStar = false;

	while (*f && (hasStar || *p)) {
		if (*f == '%') {
			f++; // 跳过 %

			// 检查 * 标志（跳过赋值）
			int suppress = 0;
			if (*f == '*') {
				hasStar = true;
				suppress = 1;
				f++;
			}

			// 解析字段宽度
			int width = parse_number(&f);

			// 解析精度（暂未使用）
			int precision = -1;
			if (*f == '.') {
				f++;
				if (*f == '*') {
					precision = va_arg(args, int);
					f++;
				}
				else {
					precision = parse_number(&f);
				}
			}

			// 检查长度修饰符
			char length_mod[4] = { 0 }; // 支持 hh, h, l, ll, I64
			if (*f == 'h') {
				length_mod[0] = *f++;
				if (*f == 'h') length_mod[1] = *f++; // hh
			}
			else if (*f == 'l') {
				length_mod[0] = *f++;
				if (*f == 'l') length_mod[1] = *f++; // ll
			}
			else if (*f == 'I' && *(f + 1) == '6' && *(f + 2) == '4') {
				length_mod[0] = 'I';
				length_mod[1] = '6';
				length_mod[2] = '4';
				f += 3; // I64
			}

			// 处理格式说明符
			switch (*f) {
			case 'd': case 'i': { // 有符号十进制整数
				p = skip_whitespace(p);
				char* end;
				int64_t val = strtoll(p, &end, 10);
				if (end == p) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					if (strcmp(length_mod, "hh") == 0) {
						*va_arg(args, signed char*) = (signed char)val;
					}
					else if (length_mod[0] == 'h') {
						*va_arg(args, short*) = (short)val;
					}
					else if (strcmp(length_mod, "ll") == 0) {
						*va_arg(args, long long*) = val;
					}
					else if (strcmp(length_mod, "I64") == 0) {
						*va_arg(args, int64_t*) = val;
					}
					else if (length_mod[0] == 'l') {
						*va_arg(args, long*) = (long)val;
					}
					else {
						*va_arg(args, int*) = (int)val;
					}
					count++;
				}
				p = end;
				break;
			}
			case 'u': { // 无符号十进制整数
				p = skip_whitespace(p);
				char* end;
				uint64_t val = strtoull(p, &end, 10);
				if (end == p) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					if (strcmp(length_mod, "hh") == 0) {
						*va_arg(args, unsigned char*) = (unsigned char)val;
					}
					else if (length_mod[0] == 'h') {
						*va_arg(args, unsigned short*) = (unsigned short)val;
					}
					else if (length_mod[0] == 'l') {
						*va_arg(args, unsigned long*) = (unsigned long)val;
					}
					else {
						*va_arg(args, unsigned int*) = (unsigned int)val;
					}
					count++;
				}
				p = end;
				break;
			}
			case 'x': case 'X': { // 十六进制整数
				p = skip_whitespace(p);
				char* end;
				uint64_t val = strtoull(p, &end, 16);
				if (end == p) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					if (strcmp(length_mod, "ll") == 0) {
						*va_arg(args, unsigned long long*) = val;
					}
					else if (length_mod[0] == 'l') {
						*va_arg(args, unsigned long*) = (unsigned long)val;
					}
					else {
						*va_arg(args, unsigned int*) = (unsigned int)val;
					}
					count++;
				}
				p = end;
				break;
			}
			case 'f': { // 浮点数
				p = skip_whitespace(p);
				char* end;
				float val = strtof(p, &end);
				if (end == p) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					*va_arg(args, float*) = val;
					count++;
				}
				p = end;
				break;
			}
			case 'p': { // 指针地址（十六进制）
				p = skip_whitespace(p);
				if (!*p || (*p != '0' && *(p + 1) != 'x')) {
					throw_format_error(input, format, p, f);
					return count;
				}
				p += 2; // 跳过 "0x"
				char* end;
				uintptr_t val = (uintptr_t)strtoull(p, &end, 16);
				if (end == p) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					*va_arg(args, void**) = (void*)val;
					count++;
				}
				p = end;
				break;
			}
			case 's': { // 字符串（支持 GB2312）
				p = skip_whitespace(p);
				if (!*p && !suppress) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					char* str = va_arg(args, char*);
					size_t size = SIZE_MAX; // 默认最大缓冲区大小

					size_t i = 0;
					while (*p && !isspace((unsigned char)*p) &&
						(width == 0 || i < (size_t)width) && i < size - 1) {
						if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1) && i + 1 < size - 1) {
							str[i++] = *p++;
							str[i++] = *p++;
						}
						else {
							str[i++] = *p++;
						}
					}
					str[i] = '\0';
					if (i == 0) {
						throw_format_error(input, format, p, f);
						return count;
					}
					count++;
				}
				else {
					int read = 0;
					while (*p && !isspace((unsigned char)*p) && (width == 0 || width > 0)) {
						if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
							p += 2;
							if (width) width -= 2;
							read = 1;
						}
						else {
							p++;
							if (width) width--;
							read = 1;
						}
					}
					if (!read && *p) {
						throw_format_error(input, format, p, f);
						return count;
					}
				}
				break;
			}
			case '[': { // 扫描集（支持 %[...] 和 %[^...]）
				f++; // 跳过 [
				int invert = 0;


				if (*f == '^') {
					invert = 1;
					f++;
				}
				char charset[256] = { 0 };
				int first = 1;
				while (*f && (*f != ']' || first)) {
					first = 0;
					if (*f == '-' && !first && *(f + 1) != ']') {
						char start = *(f - 1);
						char end = *(f + 1);
						for (char c = start; c <= end; c++) {
							charset[(unsigned char)c] = 1;
						}
						f += 2;
					}
					else {
						charset[(unsigned char)*f] = 1;
						f++;
					}
				}
				if (*f != ']') {
					throw_format_error(input, format, p, f);
					return 0;
				}
				f++; // 跳过 ]

				if (!suppress) {
					char* str = va_arg(args, char*);
					size_t size = SIZE_MAX; // 默认最大缓冲区大小

					size_t i = 0;
					while (*p && (width == 0 || i < (size_t)width) && i < size - 1) {
						int match = invert ? !charset[(unsigned char)*p] : charset[(unsigned char)*p];
						if (!match) break;
						if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1) && i + 1 < size - 1) {
							str[i++] = *p++;
							str[i++] = *p++;
						}
						else {
							str[i++] = *p++;
						}
					}
					str[i] = '\0';
					if (i > 0) {
						count++;
					}
					else {
						p = skip_whitespace(p);
						while (*p && !isspace((unsigned char)*p) && charset[(unsigned char)*p]) {
							p++;
						}
						p = skip_whitespace(p);
					}
				}
				else {
					int read = 0;
					while (*p && (width == 0 || width > 0)) {
						int match = invert ? !charset[(unsigned char)*p] : charset[(unsigned char)*p];
						if (!match) break;
						if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
							p += 2;
							if (width) width -= 2;
							read = 1;
						}
						else {
							p++;
							if (width) width--;
							read = 1;
						}
					}
					if (!read) {
						p = skip_whitespace(p);
						while (*p && !isspace((unsigned char)*p) && charset[(unsigned char)*p]) {
							p++;
						}
						p = skip_whitespace(p);
					}
				}
				break;
			}
			case 'c': { // 单个字符（支持 GB2312）
				if (!*p && !suppress) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					char* ch = va_arg(args, char*);
					if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
						*ch++ = *p++;
						*ch = *p++;
					}
					else {
						*ch = *p++;
					}
					count++;
				}
				else {
					if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
						p += 2;
					}
					else {
						p++;
					}
				}
				break;
			}
			case 'n': { // 记录已读取的字符数
				if (!suppress) {
					*va_arg(args, int*) = (int)(p - input);
				}
				break;
			}
			case '%': {
				if (*p++ != '%') {
					throw_format_error(input, format, p - 1, f);
					return count;
				}
				break;
			}
			default:
				throw_format_error(input, format, p, f);
				return count;
			}
			f++;
		}
		else {
			if (isspace(*f)) {
				f++;
				p = skip_whitespace(p);
			}
			else {
				if (*f++ != *p++) {
					throw_format_error(input, format, p - 1, f - 1);
					return count;
				}
			}
		}
	}

	va_end(args);
	return count;
}
int my_sscanf(const char* input, const char* format, ...) {
	va_list args;
	va_start(args, format);
	return my_vsscanf(input, format, args);
}

// 支持 GB2312 和扩展格式的 my_sscanf_s 函数，%s 和 %[...] 必须提供 sizeof(str)
int my_vsscanf_s(const char* input, const char* format, va_list args) {
	int count = 0;          // 成功解析的参数计数
	const char* p = input;  // 输入字符串指针
	const char* f = format; // 格式字符串指针
	bool hasStar = false;

	while (*f && (hasStar || *p)) {
		if (*f == '%') {
			f++; // 跳过 %

			// 检查 * 标志（跳过赋值）
			int suppress = 0;
			if (*f == '*') {
				hasStar = true;
				suppress = 1;
				f++;
			}

			// 解析字段宽度
			int width = parse_number(&f);

			// 解析精度（暂未使用）
			int precision = -1;
			if (*f == '.') {
				f++;
				if (*f == '*') {
					precision = va_arg(args, int);
					f++;
				}
				else {
					precision = parse_number(&f);
				}
			}

			// 检查长度修饰符
			char length_mod[4] = { 0 }; // 支持 hh, h, l, ll, I64
			if (*f == 'h') {
				length_mod[0] = *f++;
				if (*f == 'h') length_mod[1] = *f++; // hh
			}
			else if (*f == 'l') {
				length_mod[0] = *f++;
				if (*f == 'l') length_mod[1] = *f++; // ll
			}
			else if (*f == 'I' && *(f + 1) == '6' && *(f + 2) == '4') {
				length_mod[0] = 'I';
				length_mod[1] = '6';
				length_mod[2] = '4';
				f += 3; // I64
			}

			// 处理格式说明符
			switch (*f) {
			case 'd': case 'i': { // 有符号十进制整数
				p = skip_whitespace(p);
				char* end;
				int64_t val = strtoll(p, &end, 10);
				if (end == p) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					if (strcmp(length_mod, "hh") == 0) {
						*va_arg(args, signed char*) = (signed char)val;
					}
					else if (length_mod[0] == 'h') {
						*va_arg(args, short*) = (short)val;
					}
					else if (strcmp(length_mod, "ll") == 0) {
						*va_arg(args, long long*) = val;
					}
					else if (strcmp(length_mod, "I64") == 0) {
						*va_arg(args, int64_t*) = val;
					}
					else if (length_mod[0] == 'l') {
						*va_arg(args, long*) = (long)val;
					}
					else {
						*va_arg(args, int*) = (int)val;
					}
					count++;
				}
				p = end;
				break;
			}
			case 'u': { // 无符号十进制整数
				p = skip_whitespace(p);
				char* end;
				uint64_t val = strtoull(p, &end, 10);
				if (end == p) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					if (strcmp(length_mod, "hh") == 0) {
						*va_arg(args, unsigned char*) = (unsigned char)val;
					}
					else if (length_mod[0] == 'h') {
						*va_arg(args, unsigned short*) = (unsigned short)val;
					}
					else if (length_mod[0] == 'l') {
						*va_arg(args, unsigned long*) = (unsigned long)val;
					}
					else {
						*va_arg(args, unsigned int*) = (unsigned int)val;
					}
					count++;
				}
				p = end;
				break;
			}
			case 'x': case 'X': { // 十六进制整数
				p = skip_whitespace(p);
				char* end;
				uint64_t val = strtoull(p, &end, 16);
				if (end == p) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					if (strcmp(length_mod, "ll") == 0) {
						*va_arg(args, unsigned long long*) = val;
					}
					else if (length_mod[0] == 'l') {
						*va_arg(args, unsigned long*) = (unsigned long)val;
					}
					else {
						*va_arg(args, unsigned int*) = (unsigned int)val;
					}
					count++;
				}
				p = end;
				break;
			}
			case 'f': { // 浮点数
				p = skip_whitespace(p);
				char* end;
				float val = strtof(p, &end);
				if (end == p) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					*va_arg(args, float*) = val;
					count++;
				}
				p = end;
				break;
			}
			case 'p': { // 指针地址（十六进制）
				p = skip_whitespace(p);
				if (!*p || (*p != '0' && *(p + 1) != 'x')) {
					throw_format_error(input, format, p, f);
					return count;
				}
				p += 2; // 跳过 "0x"
				char* end;
				uintptr_t val = (uintptr_t)strtoull(p, &end, 16);
				if (end == p) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					*va_arg(args, void**) = (void*)val;
					count++;
				}
				p = end;
				break;
			}
			case 's': { // 字符串（支持 GB2312）
				p = skip_whitespace(p);
				if (!*p && !suppress) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					char* str = va_arg(args, char*);
					size_t size = va_arg(args, size_t); // 必须提供缓冲区大小

					size_t i = 0;
					while (*p && !isspace((unsigned char)*p) &&
						(width == 0 || i < (size_t)width) && i < size - 1) {
						if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1) && i + 1 < size - 1) {
							str[i++] = *p++;
							str[i++] = *p++;
						}
						else {
							str[i++] = *p++;
						}
					}
					str[i] = '\0';
					if (i == 0) {
						throw_format_error(input, format, p, f);
						return count;
					}
					count++;
				}
				else {
					int read = 0;
					while (*p && !isspace((unsigned char)*p) && (width == 0 || width > 0)) {
						if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
							p += 2;
							if (width) width -= 2;
							read = 1;
						}
						else {
							p++;
							if (width) width--;
							read = 1;
						}
					}
					if (!read && *p) {
						throw_format_error(input, format, p, f);
						return count;
					}
				}
				break;
			}
			case '[': { // 扫描集（支持 %[...] 和 %[^...]）
				f++; // 跳过 [
				int invert = 0;
				if (*f == '^') {
					invert = 1;
					f++;
				}
				char charset[256] = { 0 };
				int first = 1;
				while (*f && (*f != ']' || first)) {
					first = 0;
					if (*f == '-' && !first && *(f + 1) != ']') {
						char start = *(f - 1);
						char end = *(f + 1);
						for (char c = start; c <= end; c++) {
							charset[(unsigned char)c] = 1;
						}
						f += 2;
					}
					else {
						charset[(unsigned char)*f] = 1;
						f++;
					}
				}
				if (*f != ']') {
					throw_format_error(input, format, p, f);
					return 0;
				}
				f++; // 跳过 ]

				if (!suppress) {
					char* str = va_arg(args, char*);
					size_t size = va_arg(args, size_t); // 必须提供缓冲区大小

					size_t i = 0;
					while (*p && (width == 0 || i < (size_t)width) && i < size - 1) {
						int match = invert ? !charset[(unsigned char)*p] : charset[(unsigned char)*p];
						if (!match) break;
						if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1) && i + 1 < size - 1) {
							str[i++] = *p++;
							str[i++] = *p++;
						}
						else {
							str[i++] = *p++;
						}
					}
					str[i] = '\0';
					if (i > 0) {
						count++;
					}
					else {
						p = skip_whitespace(p);
						while (*p && !isspace((unsigned char)*p) && charset[(unsigned char)*p]) {
							p++;
						}
						p = skip_whitespace(p);
					}
				}
				else {
					int read = 0;
					while (*p && (width == 0 || width > 0)) {
						int match = invert ? !charset[(unsigned char)*p] : charset[(unsigned char)*p];
						if (!match) break;
						if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
							p += 2;
							if (width) width -= 2;
							read = 1;
						}
						else {
							p++;
							if (width) width--;
							read = 1;
						}
					}
					if (!read) {
						p = skip_whitespace(p);
						while (*p && !isspace((unsigned char)*p) && charset[(unsigned char)*p]) {
							p++;
						}
						p = skip_whitespace(p);
					}
				}
				break;
			}
			case 'c': { // 单个字符（支持 GB2312）
				if (!*p && !suppress) {
					throw_format_error(input, format, p, f);
					return count;
				}
				if (!suppress) {
					char* ch = va_arg(args, char*);
					if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
						*ch++ = *p++;
						*ch = *p++;
					}
					else {
						*ch = *p++;
					}
					count++;
				}
				else {
					if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
						p += 2;
					}
					else {
						p++;
					}
				}
				break;
			}
			case 'n': { // 记录已读取的字符数
				if (!suppress) {
					*va_arg(args, int*) = (int)(p - input);
				}
				break;
			}
			case '%': {
				if (*p++ != '%') {
					throw_format_error(input, format, p - 1, f);
					return count;
				}
				break;
			}
			default:
				throw_format_error(input, format, p, f);
				return count;
			}
			f++;
		}
		else {
			if (isspace(*f)) {
				f++;
				p = skip_whitespace(p);
			}
			else {
				if (*f++ != *p++) {
					throw_format_error(input, format, p - 1, f - 1);
					return count;
				}
			}
		}
	}

	va_end(args);
	return count;
}
int my_sscanf_s(const char* input, const char* format, ...) {
	va_list args;
	va_start(args, format);
	return my_vsscanf_s(input, format, args);
}


// 从文件中解析当前一行的 my_fscanf，无需 sizeof(str)
int my_fscanf(const FILE* fp, const char* format, ...) {
	va_list args;
	va_start(args, format);
	int count = 0; // 当前行解析项数
	char buffer[1024]; // 行缓冲区

	// 文件指针必须有效
	if (!fp) {
		va_end(args);
		return -1; // 文件无效，返回 -1
	}

	// 读取当前行
	if (fgets(buffer, sizeof(buffer), (FILE*)fp) == NULL) {
		va_end(args);
		return 0; // 读取失败或到达文件末尾，返回 0
	}

	// 移除末尾换行符
	size_t len = strlen(buffer);
	if (len > 0 && buffer[len - 1] == '\n') {
		buffer[len - 1] = '\0';
	}

	// 创建 va_list 副本
	va_list args_copy;
	va_copy(args_copy, args);

	// 调用 my_sscanf 解析当前行
	count = my_vsscanf(buffer, format, args_copy);

	va_end(args_copy);
	va_end(args);
	return count;
}

// 从文件中解析当前一行的 my_fscanf_s，%s 和 %[...] 必须提供 sizeof(str)
int my_fscanf_s(const FILE* fp, const char* format, ...) {
	va_list args;
	va_start(args, format);
	int count = 0; // 当前行解析项数
	char buffer[1024]; // 行缓冲区

	// 文件指针必须有效
	if (!fp) {
		va_end(args);
		return -1; // 文件无效，返回 -1
	}

	// 读取当前行
	if (fgets(buffer, sizeof(buffer), (FILE*)fp) == NULL) {
		va_end(args);
		return 0; // 读取失败或到达文件末尾，返回 0
	}

	// 移除末尾换行符
	size_t len = strlen(buffer);
	if (len > 0 && buffer[len - 1] == '\n') {
		buffer[len - 1] = '\0';
	}
	// 创建 va_list 副本
	va_list args_copy;
	va_copy(args_copy, args);
	// 调用 my_sscanf_s 解析当前行
	count = my_vsscanf_s(buffer, format, args_copy);

	va_end(args);
	return count;
}


// 测试代码
int main() {

	FILE* fp = fopen("test.txt", "r");
	if (!fp) {
		perror("Failed to open file");
		return 1;
	}

	void* ptr;
	int num;
	char str[20];

	// 解析第一行
	int result = my_fscanf(fp, "%p %d", &ptr, &num);
	printf("my_fscanf Line 1 - Parsed items: %d, ptr: %#lx, num: %d\n", result, (unsigned long)ptr, num);

	// 解析第二行
	result = my_fscanf(fp, "%s", str);
	printf("my_fscanf Line 2 - Parsed items: %d, str: %s\n", result, str);

	//fclose(fp);
	
	freopen("test.txt", "r", fp);
	if (!fp) {
		perror("Failed to open file");
		return 1;
	}

	int resultf2 = my_fscanf_s(fp, "%p %d", &ptr, &num);
	printf("my_fscanf_s Line 1 - Parsed items: %d, ptr: %#lx, num: %d\n", resultf2, (unsigned long)ptr, num);

	// 解析第二行
	resultf2 = my_fscanf_s(fp, "%s", str, sizeof(str));
	printf("my_fscanf_s Line 2 - Parsed items: %d, str: %s\n", resultf2, str);

	return 0;

#if 0
	// 原有测试用例
	printf("=== Original Tests ===\n");
	char str[20];
	int result;

	// Test 1: %[^#]
	strcpy(str, "");
	result = my_sscanf("hello#world", "%[^#]", str);
	printf("Test 1 - Parsed items: %d, str: %s\n", result, str);

	strcpy(str, "");
	result = my_sscanf_s("hello world", "%s", str, sizeof(str));
	printf("Test S1 - Parsed items: %d, str: %s (expect 'hello')\n", result, str);

	// Test 2: %[^$]
	strcpy(str, "");
	result = my_sscanf("price$100", "%[^$]", str);
	printf("Test 2 - Parsed items: %d, str: %s\n", result, str);

	// Test S2: %s 缓冲区大小限制
	strcpy(str, "");
	result = my_sscanf_s("verylongstring here", "%s", str, 6); // 限制为 6 字节
	printf("Test S2 - Parsed items: %d, str: %s (expect 'veryl')\n", result, str);

	// Test 3: %[^]]
	strcpy(str, "");
	result = my_sscanf("text]end", "%[^]]", str);
	printf("Test 3 - Parsed items: %d, str: %s\n", result, str);

	// Test S3: %[...] 正常解析
	strcpy(str, "");
	result = my_sscanf_s("abc123 def", "%[a-z]", str, sizeof(str));
	printf("Test S3 - Parsed items: %d, str: %s (expect 'abc')\n", result, str);

	// Test 4: %[^abc]
	strcpy(str, "");
	result = my_sscanf("xyzabc123", "%[^abc]", str);
	printf("Test 4 - Parsed items: %d, str: %s\n", result, str);

	
	// Test 5: %[^a-z]
	strcpy(str, "");
	result = my_sscanf("HELLOworld", "%[^a-z]", str);
	printf("Test 5 - Parsed items: %d, str: %s\n", result, str);


	// Test 6: %[^a-zA-Z0-9#]
	strcpy(str, "");
	int ddd = 0;
	result = my_sscanf("123 hello123#world ", "%d %[^a-zA-Z0-9#] ", &ddd,str);
	printf("Test 6 - Parsed items: %d, str: \"%s\", ddd: %d\n", result, str, ddd);

	// Test S4: %[^...] 无匹配后跳跃
	strcpy(str, "");
	result = my_sscanf_s("hello123#world 123", "%[^a-zA-Z0-9#] %d", str, sizeof(str), &ddd);
	printf("Test S4 - Parsed items: %d, str: \"%s\", ddd: %d (expect '', 123)\n", result, str, ddd);


	// Test S5: GB2312 字符串
	strcpy(str, "");
	result = my_sscanf_s("\xC4\xE3\xBA\xC3 world", "%s", str, sizeof(str)); // "你好 world"
	printf("Test S5 - Parsed items: %d, str: %s (expect '你好')\n", result, str);

	// Test S6: %s 缓冲区不足以容纳 GB2312
	char small_str[3];
	strcpy(small_str, "");
	result = my_sscanf_s("\xC4\xE3\xBA\xC3 world", "%s", small_str, sizeof(small_str));
	printf("Test S6 - Parsed items: %d, str: %s (expect truncated or empty)\n", result, small_str);

	// Test S7: %[...] 超长输入
	strcpy(str, "");
	result = my_sscanf_s("abcdefghijklmnopqrstuvwxyz", "%[a-z]", str, 5); // 限制为 5 字节
	printf("Test S7 - Parsed items: %d, str: %s (expect 'abcd')\n", result, str);

	// Test S8: 空输入
	strcpy(str, "");
	result = my_sscanf_s("", "%s", str, sizeof(str));
	printf("Test S8 - Parsed items: %d, str: %s (expect error, 0 items)\n", result, str);

	// Test S9: %s 和 %d 混合，部分匹配
	strcpy(str, "");
	int num = 0;
	result = my_sscanf_s("hello 42", "%s %d", str, sizeof(str), &num);
	printf("Test S9 - Parsed items: %d, str: %s, num: %d (expect 'hello', 42)\n", result, str, num);

	// Test S10: %[...] 无匹配，无后续
	strcpy(str, "");
	result = my_sscanf_s("123abc", "%[a-z]", str, sizeof(str));
	printf("Test S10 - Parsed items: %d, str: %s (expect '', 0 items)\n", result, str);

	// Test 7: %[^=] with sizeof
	strcpy(str, "");
	result = my_sscanf("key=value", "%[^=]", str, sizeof(str));
	printf("Test 7 - Parsed items: %d, str: %s\n", result, str);

	// Test 8: GB2312 %[^=]
	strcpy(str, "");
	result = my_sscanf("\xC4\xE3\xBA\xC3=value", "%[^=]", str);
	printf("Test 8 - Parsed items: %d, str: %s\n", result, str);

	// 新增测试用例
	printf("\n=== Additional Normal Tests ===\n");

	// Test 9: %d (整数)
	int d;
	result = my_sscanf("123 abc", "%d", &d);
	printf("Test 9 - Parsed items: %d, d: %d\n", result, d);

	// Test 10: %u (无符号整数)
	unsigned int u;
	result = my_sscanf("456 def", "%u", &u);
	printf("Test 10 - Parsed items: %d, u: %u\n", result, u);

	// Test 11: %x (十六进制)
	unsigned int x;
	result = my_sscanf("1a2b ghi", "%x", &x);
	printf("Test 11 - Parsed items: %d, x: %x\n", result, x);

	// Test 12: %f (浮点数)
	float fval;
	result = my_sscanf("3.14 jkl", "%f", &fval);
	printf("Test 12 - Parsed items: %d, f: %.2f\n", result, fval);

	// Test 13: %s (普通字符串)
	strcpy(str, "");
	result = my_sscanf("hello world", "%s", str);
	printf("Test 13 - Parsed items: %d, str: %s\n", result, str);

	// Test 14: %[a-z] (扫描集)
	strcpy(str, "");
	result = my_sscanf("abcdef123", "%[a-z]", str);
	printf("Test 14 - Parsed items: %d, str: %s\n", result, str);

	// Test 15: %c (单个字符)
	char c;
	result = my_sscanf("xyz", "%c", &c);
	printf("Test 15 - Parsed items: %d, c: %c\n", result, c);

	// Test 16: %n (已读取字符数)
	int n;
	result = my_sscanf("abc123", "%*s%n", &n);
	printf("Test 16 - Parsed items: %d, n: %d\n", result, n);

	// Test 17: %% (匹配 %)
	result = my_sscanf("100% complete", "100%% complete");
	printf("Test 17 - Parsed items: %d (should be 0, no assignment)\n", result);

	// Test 18: 宽度限制 %5s
	strcpy(str, "");
	result = my_sscanf("abcdefgh", "%5s", str);
	printf("Test 18 - Parsed items: %d, str: %s\n", result, str);

	// Test 19: 混合格式
	float fnum;
	strcpy(str, "");
	result = my_sscanf("42 3.14 hello", "%d %f %s", &num, &fnum, str);
	printf("Test 19 - Parsed items: %d, num: %d, fnum: %.2f, str: %s\n", result, num, fnum, str);

	printf("\n=== Additional Abnormal Tests ===\n");

	// Test 20: %d 无效输入
	result = my_sscanf("abc", "%d", &d);
	printf("Test 20 - Parsed items: %d (expect 0, invalid int)\n", result);

	// Test 21: %f 无效输入
	result = my_sscanf("xyz", "%f", &fval);
	printf("Test 21 - Parsed items: %d (expect 0, invalid float)\n", result);

	// Test 22: %s 空输入
	strcpy(str, "");
	result = my_sscanf("", "%s", str);
	printf("Test 22 - Parsed items: %d, str: %s (expect 0, empty input)\n", result, str);

	// Test 23: %[a-z] 无匹配
	strcpy(str, "");
	result = my_sscanf("123", "%[a-z]", str);
	printf("Test 23 - Parsed items: %d, str: %s (expect 0, no match)\n", result, str);

	// Test 24: %[^a] 异常输入
	strcpy(str, "");
	result = my_sscanf("aaa", "%[^a]", str);
	printf("Test 24 - Parsed items: %d, str: %s (expect 0, all excluded)\n", result, str);

	// Test 25: 格式字符串不完整 %[
	strcpy(str, "");
	result = my_sscanf("abc", "%[", str);
	printf("Test 25 - Parsed items: %d (expect error)\n", result);

	// Test 26: 非法格式符 %z
	result = my_sscanf("123", "%z", &d);
	printf("Test 26 - Parsed items: %d (expect error)\n", result);

	// Test 27: 输入不足
	result = my_sscanf("42", "%d %f", &num, &fnum);
	printf("Test 27 - Parsed items: %d (expect 1, partial match)\n", result);


	// 新增长度修饰符测试用例
	printf("\n=== Length Modifier Tests ===\n");

	// Test 28: %hhd (signed char)
	signed char hh_val;
	result = my_sscanf("127 abc", "%hhd", &hh_val);
	printf("Test 28 - Parsed items: %d, hh_val: %d\n", result, hh_val);

	// Test 29: %hd (short)
	short h_val;
	result = my_sscanf("-12345 def", "%hd", &h_val);
	printf("Test 29 - Parsed items: %d, h_val: %d\n", result, h_val);

	// Test 30: %ld (long)
	long l_val;
	result = my_sscanf("2147483647 ghi", "%ld", &l_val);
	printf("Test 30 - Parsed items: %d, l_val: %ld\n", result, l_val);

	// Test 31: %lld (long long)
	long long ll_val;
	result = my_sscanf("-9223372036854775807 jkl", "%lld", &ll_val);
	printf("Test 31 - Parsed items: %d, ll_val: %lld\n", result, ll_val);

	// Test 32: %I64d (int64_t)
	int64_t i64_val;
	result = my_sscanf("9223372036854775807 mno", "%I64d", &i64_val);
	printf("Test 32 - Parsed items: %d, i64_val: %lld\n", result, i64_val);

	// Test 33: %hhu (unsigned char)
	unsigned char hhu_val;
	result = my_sscanf("255 pqr", "%hhu", &hhu_val);
	printf("Test 33 - Parsed items: %d, hhu_val: %u\n", result, hhu_val);

	// Test 34: %hu (unsigned short)
	unsigned short hu_val;
	result = my_sscanf("65535 stu", "%hu", &hu_val);
	printf("Test 34 - Parsed items: %d, hu_val: %u\n", result, hu_val);

	// Test 35: %lu (unsigned long)
	unsigned long lu_val;
	result = my_sscanf("4294967295 vwx", "%lu", &lu_val);
	printf("Test 35 - Parsed items: %d, lu_val: %lu\n", result, lu_val);

	// Test 36: %llx (long long hex)
	unsigned long long llx_val;
	result = my_sscanf("ffffFFFFffffFFFF yz", "%llx", &llx_val);
	printf("Test 36 - Parsed items: %d, llx_val: %llx\n", result, llx_val);


	// === Additional Tests for %p ===
	printf("\n=== Additional Tests for %%p ===\n");

	// Test P1
	void* ptr1;
	result = my_sscanf("0x12345678", "%p", &ptr1);
	printf("Test P1 - Parsed items: %d, ptr: %#lx (expect 0x12345678)\n", result, (unsigned long)ptr1);

	// Test P2
	void* ptr2;
	result = my_sscanf_s("0xabcdef", "%p", &ptr2);
	printf("Test P2 - Parsed items: %d, ptr: %#lx (expect 0xabcdef)\n", result, (unsigned long)ptr2);

	// Test P3: %p 无效输入
	void* ptr3;
	result = my_sscanf("12345678", "%p", &ptr3);
	printf("Test P3 - Parsed items: %d (expect error, 0 items)\n", result);

	// Test P4: %p 带 * 抑制赋值
	result = my_sscanf("0xdeadbeef", "%*p");
	printf("Test P4 - Parsed items: %d (expect 0 items)\n", result);

	// Test P5
	void* ptr5;
	int num5 = 0;
	result = my_sscanf_s("0x1234 5678", "%p %d", &ptr5, &num5);
	printf("Test P5 - Parsed items: %d, ptr: %#lx, num: %d (expect 0x1234, 5678)\n", result, (unsigned long)ptr5, num5);

	return 0;
#endif

}