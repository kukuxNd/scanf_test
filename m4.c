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

// 抛出异常提醒函数
static void throw_format_error(const char *input, const char *format, const char *p, const char *f) {
    fprintf(stderr, "Format mismatch error:\n");
    fprintf(stderr, "  Input: %s\n", input);
    fprintf(stderr, "  Format: %s\n", format);
    fprintf(stderr, "  Position: input[%ld]='%c', format[%ld]='%c'\n",
            (long)(p - input), *p ? *p : '\0', (long)(f - format), *f);
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
            char length_mod[3] = {0}; // 支持 hh, h, l, ll, I64
            if (*f == 'h') {
                length_mod[0] = *f++;
                if (*f == 'h') {
                    length_mod[1] = *f++; // hh
                }
            } else if (*f == 'l') {
                length_mod[0] = *f++;
                if (*f == 'l') {
                    length_mod[1] = *f++; // ll
                }
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
                    if (end == p) {
                        throw_format_error(input, format, p, f);
                        return count;
                    }
                    if (!suppress) {
                        if (strcmp(length_mod, "hh") == 0) {
                            signed char *ptr = va_arg(args, signed char *);
                            *ptr = (signed char)val;
                        } else if (length_mod[0] == 'h') {
                            short *ptr = va_arg(args, short *);
                            *ptr = (short)val;
                        } else if (strcmp(length_mod, "ll") == 0) {
                            long long *ptr = va_arg(args, long long *);
                            *ptr = (long long)val;
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
                    if (end == p) {
                        throw_format_error(input, format, p, f);
                        return count;
                    }
                    if (!suppress) {
                        if (length_mod[0] == 'h') {
                            unsigned short *ptr = va_arg(args, unsigned short *);
                            *ptr = (unsigned short)val;
                        } else if (length_mod[0] == 'l') {
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
                    if (end == p) {
                        throw_format_error(input, format, p, f);
                        return count;
                    }
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
                        size_t size = SIZE_MAX; // 默认无限制
                        // 尝试读取 size_t 参数
                        va_list args_copy;
                        va_copy(args_copy, args);
                        void *next_arg = va_arg(args_copy, void *);
                        if (next_arg == str) {
                            size = SIZE_MAX;
                        } else {
                            size = va_arg(args, size_t); // 读取 size_t
                        }
                        va_end(args_copy);

                        size_t i = 0;
                        while (*p && !isspace((unsigned char)*p) && 
                               (width == 0 || i < (size_t)width) && i < size - 1) {
                            if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1) && i + 1 < size - 1) {
                                str[i++] = *p++;
                                str[i++] = *p++;
                            } else {
                                str[i++] = *p++;
                            }
                        }
                        str[i] = '\0';
                        if (i == 0) {
                            throw_format_error(input, format, p, f);
                            return count;
                        }
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
                        if (!read) {
                            throw_format_error(input, format, p, f);
                            return count;
                        }
                    }
                    break;
                }
                case '[': { // 扫描集（支持 %[^\n]）
                    f++; // 跳过 [
                    int invert = 0;
                    if (*f == '^') {
                        invert = 1;
                        f++;
                    }
                    char charset[256] = {0};
                    if (*f == '\n' && *(f + 1) == ']') { // 处理 %[^\n]
                        charset['\n'] = 1;
                        f += 2; // 跳过 \n]
                    } else {
                        while (*f && *f != ']') {
                            if (*f == '-' && f > format + 1 && *(f + 1) != ']') {
                                char start = *(f - 1);
                                char end = *(f + 1);
                                for (char c = start; c <= end; c++) {
                                    charset[(unsigned char)c] = 1;
                                }
                                f += 2;
                            } else {
                                charset[(unsigned char)*f] = 1;
                                f++;
                            }
                        }
                        if (*f == ']') f++; // 跳过 ]
                        else {
                            throw_format_error(input, format, p, f);
                            return count;
                        }
                    }

                    if (!suppress) {
                        char *str = va_arg(args, char *);
                        size_t size = SIZE_MAX; // 默认无限制
                        // 尝试读取 size_t 参数
                        va_list args_copy;
                        va_copy(args_copy, args);
                        void *next_arg = va_arg(args_copy, void *);
                        if (next_arg == str) {
                            size = SIZE_MAX;
                        } else {
                            size = va_arg(args, size_t); // 读取 size_t
                        }
                        va_end(args_copy);

                        size_t i = 0;
                        while (*p && (width == 0 || i < (size_t)width) && i < size - 1) {
                            if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
                                unsigned char lead = (unsigned char)*p;
                                unsigned char trail = (unsigned char)*(p + 1);
                                int match = invert ? !charset[lead] : charset[lead];
                                if (!match) break;
                                if (i + 1 >= size - 1) break;
                                str[i++] = *p++;
                                str[i++] = *p++;
                            } else {
                                int match = invert ? !charset[(unsigned char)*p] : charset[(unsigned char)*p];
                                if (!match) break;
                                str[i++] = *p++;
                            }
                        }
                        str[i] = '\0';
                        if (i == 0) {
                            throw_format_error(input, format, p, f - 1);
                            return count;
                        }
                        count++;
                    } else {
                        int read = 0;
                        while (*p && (width == 0 || width > 0)) {
                            if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
                                unsigned char lead = (unsigned char)*p;
                                int match = invert ? !charset[lead] : charset[lead];
                                if (!match) break;
                                p += 2;
                                if (width) width -= 2;
                                read = 1;
                            } else {
                                int match = invert ? !charset[(unsigned char)*p] : charset[(unsigned char)*p];
                                if (!match) break;
                                p++;
                                if (width) width--;
                                read = 1;
                            }
                        }
                        if (!read) {
                            throw_format_error(input, format, p, f - 1);
                            return count;
                        }
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
                    if (*p++ != '%') {
                        throw_format_error(input, format, p - 1, f);
                        return count;
                    }
                    break;
                }
                default:
                    throw_format_error(input, format, p, f);
                    return count; // 未识别的格式说明符
            }
            f++;
        } else {
            if (isspace(*f)) {
                f++;
                p = skip_whitespace(p);
            } else {
                if (*f++ != *p++) {
                    throw_format_error(input, format, p - 1, f - 1);
                    return count; // 普通字符不匹配
                }
            }
        }
    }

    va_end(args);
    return count;
}

// 测试代码
int main() {
    // 测试成功解析（包括 %lld）
    const char *input1 = "127 -32768 65535 123456789012 4294967295 deadbeef";
    signed char hhd;
    short hd;
    unsigned short hu;
    long long lld;
    unsigned int u;
    unsigned long lx;

    int result1 = my_sscanf(input1, "%hhd %hd %hu %lld %u %lx",
                            &hhd, &hd, &hu, &lld, &u, &lx);
    printf("Test 1 - Parsed items: %d\n", result1);
    printf("hhd: %d\n", hhd);
    printf("hd: %d\n", hd);
    printf("hu: %u\n", hu);
    printf("lld: %lld\n", lld);
    printf("u: %u\n", u);
    printf("lx: %lx\n\n", lx);

    // 测试 %[^\n]（带 sizeof）
    const char *input2 = "hello world\n123";
    char str1[20];
    int d1;
    int result2 = my_sscanf(input2, "%[^\n] %d", str1, sizeof(str1), &d1);
    printf("Test 2 - Parsed items: %d\n", result2);
    printf("str: %s\n", str1);
    printf("d: %d\n\n", d1);

    // 测试 %[^\n]（无 sizeof）
    const char *input3 = "goodbye world\n456";
    char str2[20];
    int d2;
    int result3 = my_sscanf(input3, "%[^\n] %d", str2, &d2);
    printf("Test 3 - Parsed items: %d\n", result3);
    printf("str: %s\n", str2);
    printf("d: %d\n\n", d2);

    // 测试 GB2312 和 %s
    const char *input4 = "\xC4\xE3\xBA\xC3 789"; // 你好 789
    char str3[10];
    int d3;
    int result4 = my_sscanf(input4, "%s %d", str3, sizeof(str3), &d3);
    printf("Test 4 - Parsed items: %d\n", result4);
    printf("str: %s\n", str3);
    printf("d: %d\n\n", d3);

    // 测试 %lld 和格式不匹配
    const char *input5 = "123 abc";
    long long lld2;
    int d4;
    int result5 = my_sscanf(input5, "%lld %d", &lld2, &d4);
    printf("Test 5 - Parsed items: %d\n", result5);
    printf("lld: %lld\n", lld2);
    printf("d: %d (undefined)\n", d4);

    const char *input6 = "12 23 34 45 56";
    int aa;
    char bb[10];
    char cc[10];
    int dd;
    int result6 = my_sscanf(input6, "%d %s %s %d", &aa, bb,cc,&dd);
    printf("Test 6 - Parsed items: %d\n", result6);
    printf("aa: %d\n", aa);
    printf("bb: %s\n", bb);
    printf("cc: %s\n", cc);
    printf("dd: %d\n", dd);


    return 0;
}