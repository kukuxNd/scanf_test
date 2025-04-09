#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

// 判断是否为 GB2312 双字节字符的首字节
static int is_gb2312_first_byte(unsigned char c) {
    return (c >= 0xA1 && c <= 0xFE);
}

// 判断是否为 GB2312 双字节字符的次字节
static int is_gb2312_second_byte(unsigned char c) {
    return (c >= 0xA1 && c <= 0xFE);
}

// 跳过空白字符（仅处理单字节空白）
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

// 扩展的 my_sscanf 函数，支持 GB2312
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

            // 检查长度修饰符
            char length_mod = 0;
            if (*f == 'h' || *f == 'l') {
                length_mod = *f;
                f++;
            }

            // 处理格式说明符
            switch (*f) {
                case 'd': case 'i': { // 有符号十进制整数
                    p = skip_whitespace(p);
                    char *end;
                    long val = strtol(p, &end, 10);
                    if (end == p) break;
                    if (!suppress) {
                        if (length_mod == 'l') {
                            long *ptr = va_arg(args, long *);
                            *ptr = val;
                        } else if (length_mod == 'h') {
                            short *ptr = va_arg(args, short *);
                            *ptr = (short)val;
                        } else {
                            int *ptr = va_arg(args, int *);
                            *ptr = (int)val;
                        }
                        count++;
                    }
                    p = end;
                    break;
                }
                case 's': { // 字符串（支持 GB2312）
                    if (!suppress) {
                        char *str = va_arg(args, char *);
                        size_t size = va_arg(args, size_t); // 缓冲区大小（字节数）
                        size_t i = 0;
                        p = skip_whitespace(p);
                        while (*p && !isspace((unsigned char)*p) && 
                               (width == 0 || i < (size_t)width) && i < size - 1) {
                            if (is_gb2312_first_byte((unsigned char)*p) && *(p + 1) && 
                                is_gb2312_second_byte((unsigned char)*(p + 1)) && i + 1 < size - 1) {
                                str[i++] = *p++; // 复制首字节
                                str[i++] = *p++; // 复制次字节
                            } else {
                                str[i++] = *p++; // 单字节字符
                            }
                        }
                        str[i] = '\0';
                        if (i > 0) count++;
                    } else {
                        p = skip_whitespace(p);
                        while (*p && !isspace((unsigned char)*p) && (width == 0 || width-- > 0)) {
                            if (is_gb2312_first_byte((unsigned char)*p) && *(p + 1)) p += 2;
                            else p++;
                        }
                    }
                    break;
                }
                case 'c': { // 单个字符（支持 GB2312）
                    if (!suppress) {
                        char *ch = va_arg(args, char *);
                        if (is_gb2312_first_byte((unsigned char)*p) && *(p + 1)) {
                            *ch++ = *p++; // 存储双字节字符的首字节
                            *ch = *p++;   // 存储次字节
                            count++;      // 算作一个字符
                        } else {
                            *ch = *p++;   // 单字节字符
                            count++;
                        }
                    } else {
                        if (is_gb2312_first_byte((unsigned char)*p) && *(p + 1)) p += 2;
                        else p++;
                    }
                    break;
                }
                case '[': { // 扫描集（支持 GB2312）
                    f++; // 跳过 [
                    int invert = 0;
                    if (*f == '^') {
                        invert = 1;
                        f++;
                    }
                    char charset[256] = {0};
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
                    if (!suppress) {
                        char *str = va_arg(args, char *);
                        size_t size = va_arg(args, size_t); // 缓冲区大小
                        size_t i = 0;
                        while (*p && (width == 0 || i < (size_t)width) && i < size - 1) {
                            if (is_gb2312_first_byte((unsigned char)*p) && *(p + 1)) {
                                unsigned char c1 = (unsigned char)*p;
                                unsigned char c2 = (unsigned char)*(p + 1);
                                // 当前实现仅支持单字节字符集匹配，GB2312 需要扩展逻辑
                                // 这里假设仅匹配单字节字符集
                                if (!invert && charset[c1]) break; // 双字节未完全支持
                                if (invert && charset[c1]) break;
                                if (i + 1 < size - 1) {
                                    str[i++] = *p++;
                                    str[i++] = *p++;
                                } else break;
                            } else {
                                unsigned char c = (unsigned char)*p;
                                if ((!invert && charset[c]) || (invert && !charset[c])) {
                                    str[i++] = *p++;
                                } else break;
                            }
                        }
                        str[i] = '\0';
                        if (i > 0) count++;
                    } else {
                        while (*p && (width == 0 || width-- > 0)) {
                            if (is_gb2312_first_byte((unsigned char)*p) && *(p + 1)) p += 2;
                            else {
                                unsigned char c = (unsigned char)*p;
                                if ((!invert && charset[c]) || (invert && !charset[c])) p++;
                                else break;
                            }
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
            } else if (*f++ != *p++) {
                break; // 普通字符不匹配
            }
        }
    }

    va_end(args);
    return count;
}

// 测试代码
int main() {
    // 示例 GB2312 编码字符串：你好 123
    const char *input = "\xC4\xE3\xBA\xC3 123"; // "你好 123" 的 GB2312 编码
    char str[10];
    int num;

    int result = my_sscanf(input, "%s %d", str, sizeof(str), &num);
    printf("Parsed items: %d\n", result);
    printf("str: %s\n", str);  // 预期输出 GB2312 编码的 "你好"
    printf("num: %d\n", num);  // 预期输出 123

    // 测试中文字符
    char ch[3] = {0};
    result = my_sscanf(input, "%c", ch);
    printf("\nParsed items: %d\n", result);
    printf("ch: %s\n", ch);    // 预期输出 GB2312 编码的 "你"

    return 0;
}