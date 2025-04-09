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
static const char *skip_whitespace(const char *p) {
    while (isspace((unsigned char)*p)) p++;
    return p;
}

// 解析字段宽度或精度
static int parse_number(const char **f) {
    int num = 0;
    while (isdigit(**f)) {
        num = num * 10 + (**f - '0');
        (*f)++;
    }
    return num;
}

// 支持 GB2312 和扩展格式的 my_sscanf 函数
int my_sscanf(const char *input, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int count = 0;          // 成功解析的参数计数
    const char *p = input;  // 输入字符串指针
    const char *f = format; // 格式字符串指针

    while (*f && *p) {
        if (*f == '%') {
            f++; // 跳过 %

            // 检查 * 标志（跳过赋值）
            int suppress = 0;
            if (*f == '*') {
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
                } else {
                    precision = parse_number(&f);
                }
            }

            // 检查长度修饰符
            char length_mod[3] = {0}; // 支持 hh, h, l, I64
            if (*f == 'h') {
                length_mod[0] = *f++;
                if (*f == 'h') {
                    length_mod[1] = *f++; // hh
                }
            } else if (*f == 'l') {
                length_mod[0] = *f++;
            } else if (*f == 'I' && *(f + 1) == '6' && *(f + 2) == '4') {
                length_mod[0] = 'I';
                length_mod[1] = '6';
                length_mod[2] = '4';
                f += 3; // I64
            }

            // 处理格式说明符
            const char *start_p = p; // 记录起始位置
            switch (*f) {
                case 'd': case 'i': { // 有符号十进制整数
                    p = skip_whitespace(p);
                    char *end;
                    int64_t val = strtoll(p, &end, 10);
                    if (end == p) return count; // 不匹配，立即返回
                    if (!suppress) {
                        if (strcmp(length_mod, "hh") == 0) {
                            signed char *ptr = va_arg(args, signed char *);
                            *ptr = (signed char)val;
                        } else if (length_mod[0] == 'h') {
                            short *ptr = va_arg(args, short *);
                            *ptr = (short)val;
                        } else if (strcmp(length_mod, "I64") == 0) {
                            int64_t *ptr = va_arg(args, int64_t *);
                            *ptr = val;
                        } else if (length_mod[0] == 'l') {
                            long *ptr = va_arg(args, long *);
                            *ptr = (long)val;
                        } else {
                            int *ptr = va_arg(args, int *);
                            *ptr = (int)val;
                        }
                        count++;
                    }
                    p = end;
                    break;
                }
                case 'u': { // 无符号十进制整数
                    p = skip_whitespace(p);
                    char *end;
                    uint64_t val = strtoull(p, &end, 10);
                    if (end == p) return count;
                    if (!suppress) {
                        if (length_mod[0] == 'l') {
                            unsigned long *ptr = va_arg(args, unsigned long *);
                            *ptr = (unsigned long)val;
                        } else {
                            unsigned int *ptr = va_arg(args, unsigned int *);
                            *ptr = (unsigned int)val;
                        }
                        count++;
                    }
                    p = end;
                    break;
                }
                case 'x': case 'X': { // 十六进制整数
                    p = skip_whitespace(p);
                    char *end;
                    uint64_t val = strtoull(p, &end, 16);
                    if (end == p) return count;
                    if (!suppress) {
                        if (length_mod[0] == 'l') {
                            unsigned long *ptr = va_arg(args, unsigned long *);
                            *ptr = (unsigned long)val;
                        } else {
                            unsigned int *ptr = va_arg(args, unsigned int *);
                            *ptr = (unsigned int)val;
                        }
                        count++;
                    }
                    p = end;
                    break;
                }
                case 's': { // 字符串（支持 GB2312）
                    p = skip_whitespace(p);
                    if (!suppress) {
                        char *str = va_arg(args, char *);
                        size_t size = va_arg(args, size_t); // 缓冲区大小（字节）
                        size_t i = 0;
                        while (*p && !isspace((unsigned char)*p) && (width == 0 || i < (size_t)width) && i < size - 1) {
                            if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1) && i + 1 < size - 1) {
                                str[i++] = *p++;
                                str[i++] = *p++;
                            } else {
                                str[i++] = *p++;
                            }
                        }
                        str[i] = '\0';
                        if (i == 0) return count; // 未读取到字符，返回
                        count++;
                    } else {
                        int read = 0;
                        while (*p && !isspace((unsigned char)*p) && (width == 0 || width > 0)) {
                            if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
                                p += 2;
                                if (width) width -= 2;
                                read = 1;
                            } else {
                                p++;
                                if (width) width--;
                                read = 1;
                            }
                        }
                        if (!read) return count; // 未读取到字符，返回
                    }
                    break;
                }
                case 'c': { // 单个字符（支持 GB2312）
                    if (!suppress) {
                        char *ch = va_arg(args, char *);
                        if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
                            *ch++ = *p++;
                            *ch = *p++;
                        } else {
                            *ch = *p++;
                        }
                        count++;
                    } else {
                        if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
                            p += 2;
                        } else {
                            p++;
                        }
                    }
                    break;
                }
                case 'n': { // 记录已读取的字符数
                    if (!suppress) {
                        int *n = va_arg(args, int *);
                        *n = p - input;
                    }
                    break;
                }
                case '%': {
                    if (*p++ != '%') return count;
                    break;
                }
                default:
                    return count; // 未识别的格式说明符
            }
            f++;
        } else {
            if (isspace(*f)) {
                f++;
                p = skip_whitespace(p);
            } else {
                if (*f++ != *p++) return count; // 普通字符不匹配
            }
        }
    }

    va_end(args);
    return count;
}

// 测试代码
int main() {
    // 测试 GB2312 字符串 (假设输入为 GB2312 编码的字节流)
    // "你好 123" 在 GB2312 中的字节表示为: 0xC4 0xE3 0xBA 0xC3 0x20 0x31 0x32 0x33
    const char *input = "\xC4\xE3\xBA\xC3 123"; // 你好 123
    char str[10];
    int d;

    int result = my_sscanf(input, "%s %d", str, sizeof(str), &d);
    printf("Parsed items: %d\n", result);
    printf("str: %s\n", str); // 应输出 GB2312 编码的 "你好"（字节流）
    printf("d: %d\n", d);

    // 测试第一个类型不匹配
    const char *input2 = "\xC4\xE3 3.14"; // 你好 3.14
    int d2;
    result = my_sscanf(input2, "%d", &d2);
    printf("\nParsed items: %d\n", result); // 应返回 0

    // 测试扫描集
    const char *input3 = "\xC4\xE3 abc"; // 你好 abc
    char str2[10];
    result = my_sscanf(input3, "%[a-zA-Z]", str2, sizeof(str2));
    printf("\nParsed items: %d\n", result); // 应返回 0（第一个不匹配）

    return 0;
}