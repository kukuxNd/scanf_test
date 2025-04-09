#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

// 跳过空白字符
static const char *skip_whitespace(const char *p) {
    while (isspace(*p)) p++;
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

// 扩展的 my_sscanf 函数
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

            // 解析精度
            int precision = -1; // 默认无精度
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
                case 'u': { // 无符号十进制整数
                    p = skip_whitespace(p);
                    char *end;
                    unsigned long val = strtoul(p, &end, 10);
                    if (end == p) break;
                    if (!suppress) {
                        if (length_mod == 'l') {
                            unsigned long *ptr = va_arg(args, unsigned long *);
                            *ptr = val;
                        } else if (length_mod == 'h') {
                            unsigned short *ptr = va_arg(args, unsigned short *);
                            *ptr = (unsigned short)val;
                        } else {
                            unsigned int *ptr = va_arg(args, unsigned int *);
                            *ptr = (unsigned int)val;
                        }
                        count++;
                    }
                    p = end;
                    break;
                }
                case 'f': case 'e': case 'g': { // 浮点数（含科学计数法）
                    p = skip_whitespace(p);
                    char *end;
                    double val = strtod(p, &end);
                    if (end == p) break;
                    if (!suppress) {
                        if (length_mod == 'l') {
                            double *ptr = va_arg(args, double *);
                            *ptr = val;
                        } else {
                            float *ptr = va_arg(args, float *);
                            *ptr = (float)val;
                        }
                        count++;
                    }
                    p = end;
                    break;
                }
                case 's': { // 字符串
                    if (!suppress) {
                        char *str = va_arg(args, char *);
                        size_t size = va_arg(args, size_t); // 缓冲区大小
                        size_t i = 0;
                        p = skip_whitespace(p);
                        while (*p && !isspace(*p) && (width == 0 || i < (size_t)width) && i < size - 1) {
                            str[i++] = *p++;
                        }
                        str[i] = '\0';
                        if (i > 0) count++;
                    } else {
                        p = skip_whitespace(p);
                        while (*p && !isspace(*p) && (width == 0 || width-- > 0)) p++;
                    }
                    break;
                }
                case 'c': { // 单个字符
                    if (!suppress) {
                        char *ch = va_arg(args, char *);
                        *ch = *p++;
                        count++;
                    } else {
                        p++;
                    }
                    break;
                }
                case 'x': case 'X': { // 十六进制整数
                    p = skip_whitespace(p);
                    char *end;
                    unsigned long val = strtoul(p, &end, 16);
                    if (end == p) break;
                    if (!suppress) {
                        if (length_mod == 'l') {
                            unsigned long *ptr = va_arg(args, unsigned long *);
                            *ptr = val;
                        } else {
                            unsigned int *ptr = va_arg(args, unsigned int *);
                            *ptr = (unsigned int)val;
                        }
                        count++;
                    }
                    p = end;
                    break;
                }
                case 'o': { // 八进制整数
                    p = skip_whitespace(p);
                    char *end;
                    unsigned long val = strtoul(p, &end, 8);
                    if (end == p) break;
                    if (!suppress) {
                        if (length_mod == 'l') {
                            unsigned long *ptr = va_arg(args, unsigned long *);
                            *ptr = val;
                        } else {
                            unsigned int *ptr = va_arg(args, unsigned int *);
                            *ptr = (unsigned int)val;
                        }
                        count++;
                    }
                    p = end;
                    break;
                }
                case 'p': { // 指针地址
                    p = skip_whitespace(p);
                    char *end;
                    unsigned long val = strtoul(p, &end, 16);
                    if (end == p) break;
                    if (!suppress) {
                        void **ptr = va_arg(args, void **);
                        *ptr = (void *)val;
                        count++;
                    }
                    p = end;
                    break;
                }
                case '[': { // 扫描集
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
                        while (*p && (invert ? !charset[(unsigned char)*p] : charset[(unsigned char)*p]) &&
                               (width == 0 || i < (size_t)width) && i < size - 1) {
                            str[i++] = *p++;
                        }
                        str[i] = '\0';
                        if (i > 0) count++;
                    } else {
                        while (*p && (invert ? !charset[(unsigned char)*p] : charset[(unsigned char)*p]) &&
                               (width == 0 || width-- > 0)) p++;
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
    const char *input = "123 3.14e-1 abcDEF 0x1A 077 0xdeadbeef";
    int d;
    float f;
    char str[10];
    int x, o;
    void *p;
    int n;

    int result = my_sscanf(input, "%d %g %[a-zA-Z] %x %o %p %n",
                           &d, &f, str, sizeof(str), &x, &o, &p, &n);
    printf("Parsed items: %d\n", result);
    printf("d: %d\n", d);
    printf("f: %.2f\n", f);
    printf("str: %s\n", str);
    printf("x: %x\n", x);
    printf("o: %o\n", o);
    printf("p: %p\n", p);
    printf("n: %d\n", n);

    const char *input2 = "[#7/12/宋体#][*8,8*]人物 [*8,-7*][~YTZS_ZFYXZ~][*-1,0*]人物 10|11|12|13|14";
    char p1[64] = {0};
    char p2[64] = {0};
    char p3[64] = {0};
    int result2 = my_sscanf(input2, "%s %s %s ",
                           p1, sizeof(p1),p2, sizeof(p2), p3, sizeof(p3));
    printf("p1: %s\n", p1);
    printf("p2: %s\n", p2);
    printf("p3: %s\n", p3);



    return 0;
}