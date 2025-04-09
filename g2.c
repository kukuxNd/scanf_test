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

// 跳过空白字符
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

// 稳定的 my_sscanf 函数
int my_sscanf(const char *input, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int count = 0;          // 成功解析的参数计数
    const char *p = input;  // 输入字符串指针
    const char *f = format; // 格式字符串指针

    while (*f) {
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

            // 解析长度修饰符（简化处理，仅支持部分）
            char length_mod[3] = {0};
            if (*f == 'h' || *f == 'l' || (*f == 'I' && *(f + 1) == '6' && *(f + 2) == '4')) {
                if (*f == 'h') {
                    length_mod[0] = *f++;
                    if (*f == 'h') length_mod[1] = *f++;
                } else if (*f == 'l') {
                    length_mod[0] = *f++;
                    if (*f == 'l') length_mod[1] = *f++;
                } else {
                    length_mod[0] = 'I';
                    length_mod[1] = '6';
                    length_mod[2] = '4';
                    f += 3;
                }
            }

            // 处理格式说明符
            switch (*f) {
                case 'd': case 'i': { // 有符号十进制整数
                    p = skip_whitespace(p);
                    if (!*p) goto end; // 输入不足，允许部分匹配
                    char *end;
                    int64_t val = strtoll(p, &end, 10);
                    if (end == p) {
                        throw_format_error(input, format, p, f);
                        goto end;
                    }
                    if (!suppress) {
                        if (length_mod[0] == 'l') *va_arg(args, long *) = (long)val;
                        else *va_arg(args, int *) = (int)val;
                        count++;
                    }
                    p = end;
                    break;
                }
                case 'u': { // 无符号十进制整数
                    p = skip_whitespace(p);
                    if (!*p) goto end;
                    char *end;
                    uint64_t val = strtoull(p, &end, 10);
                    if (end == p) {
                        throw_format_error(input, format, p, f);
                        goto end;
                    }
                    if (!suppress) {
                        if (length_mod[0] == 'l') *va_arg(args, unsigned long *) = (unsigned long)val;
                        else *va_arg(args, unsigned int *) = (unsigned int)val;
                        count++;
                    }
                    p = end;
                    break;
                }
                case 'x': case 'X': { // 十六进制整数
                    p = skip_whitespace(p);
                    if (!*p) goto end;
                    char *end;
                    uint64_t val = strtoull(p, &end, 16);
                    if (end == p) {
                        throw_format_error(input, format, p, f);
                        goto end;
                    }
                    if (!suppress) {
                        if (length_mod[0] == 'l') *va_arg(args, unsigned long *) = (unsigned long)val;
                        else *va_arg(args, unsigned int *) = (unsigned int)val;
                        count++;
                    }
                    p = end;
                    break;
                }
                case 'f': { // 浮点数
                    p = skip_whitespace(p);
                    if (!*p) goto end;
                    char *end;
                    float val = strtof(p, &end);
                    if (end == p) {
                        throw_format_error(input, format, p, f);
                        goto end;
                    }
                    if (!suppress) {
                        *va_arg(args, float *) = val;
                        count++;
                    }
                    p = end;
                    break;
                }
                case 's': { // 字符串
                    p = skip_whitespace(p);
                    if (!*p) {
                        if (!suppress) {
                            throw_format_error(input, format, p, f);
                            goto end;
                        }
                        goto end;
                    }
                    if (!suppress) {
                        char *str = va_arg(args, char *);
                        size_t i = 0;
                        while (*p && !isspace((unsigned char)*p) && (width == 0 || i < (size_t)width)) {
                            if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
                                str[i++] = *p++;
                                str[i++] = *p++;
                            } else {
                                str[i++] = *p++;
                            }
                        }
                        str[i] = '\0';
                        if (i > 0) count++;
                    } else {
                        while (*p && !isspace((unsigned char)*p) && (width == 0 || width-- > 0)) {
                            if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) p += 2;
                            else p++;
                        }
                    }
                    break;
                }
                case '[': { // 扫描集
                    f++; // 跳过 [
                    int invert = (*f == '^');
                    if (invert) f++;
                    char charset[256] = {0};
                    int first = 1;
                    const char *start_f = f; // 记录扫描集开始位置
                    while (*f && (*f != ']' || first)) {
                        first = 0;
                        if (*f == '-' && !first && *(f + 1) != ']') {
                            char start = *(f - 1);
                            char end = *(f + 1);
                            for (char c = start; c <= end; c++) charset[(unsigned char)c] = 1;
                            f += 2;
                        } else {
                            charset[(unsigned char)*f] = 1;
                            f++;
                        }
                    }
                    if (*f != ']') {
                        throw_format_error(input, format, p, f);
                        goto end;
                    }
                    f++; // 跳过 ]

                    if (!*p) {
                        if (!suppress) {
                            throw_format_error(input, format, p, start_f - 1);
                            goto end;
                        }
                        goto end;
                    }
                    if (!suppress) {
                        char *str = va_arg(args, char *);
                        size_t i = 0;
                        while (*p && (width == 0 || i < (size_t)width)) {
                            int match = invert ? !charset[(unsigned char)*p] : charset[(unsigned char)*p];
                            if (!match) break;
                            if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) {
                                str[i++] = *p++;
                                str[i++] = *p++;
                            } else {
                                str[i++] = *p++;
                            }
                        }
                        str[i] = '\0';
                        if (i == 0 && !invert && *p) {
                            throw_format_error(input, format, p, f - 1);
                            goto end;
                        }
                        if (i > 0) count++;
                    } else {
                        while (*p && (width == 0 || width-- > 0)) {
                            int match = invert ? !charset[(unsigned char)*p] : charset[(unsigned char)*p];
                            if (!match) break;
                            if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) p += 2;
                            else p++;
                        }
                    }
                    break;
                }
                case 'c': { // 单个字符
                    if (!*p) {
                        if (!suppress) {
                            throw_format_error(input, format, p, f);
                            goto end;
                        }
                        goto end;
                    }
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
                        if (is_gb2312_lead_byte((unsigned char)*p) && *(p + 1)) p += 2;
                        else p++;
                    }
                    break;
                }
                case 'n': { // 记录已读取字符数
                    if (!suppress) {
                        *va_arg(args, int *) = p - input;
                    }
                    break;
                }
                case '%': {
                    if (!*p || *p++ != '%') {
                        throw_format_error(input, format, p - 1, f);
                        goto end;
                    }
                    break;
                }
                default:
                    throw_format_error(input, format, p, f);
                    goto end;
            }
            f++; // 移动到下一个格式字符
        } else {
            if (isspace(*f)) {
                f++;
                p = skip_whitespace(p);
            } else {
                if (!*p || *f++ != *p++) {
                    throw_format_error(input, format, p - 1, f - 1);
                    goto end;
                }
            }
        }
    }

end:
    va_end(args);
    return count;
}

// 测试代码（保持不变）
int main() {
    printf("=== Original Tests ===\n");
    char str[20];
    int result;

    strcpy(str, ""); result = my_sscanf("hello#world", "%[^#]", str);
    printf("Test 1 - Parsed items: %d, str: %s\n", result, str);

    strcpy(str, ""); result = my_sscanf("price$100", "%[^$]", str);
    printf("Test 2 - Parsed items: %d, str: %s\n", result, str);

    strcpy(str, ""); result = my_sscanf("text]end", "%[^]]", str);
    printf("Test 3 - Parsed items: %d, str: %s\n", result, str);

    strcpy(str, ""); result = my_sscanf("xyzabc123", "%[^abc]", str);
    printf("Test 4 - Parsed items: %d, str: %s\n", result, str);

    strcpy(str, ""); result = my_sscanf("HELLOworld", "%[^a-z]", str);
    printf("Test 5 - Parsed items: %d, str: %s\n", result, str);

    strcpy(str, ""); result = my_sscanf("hello123#world", "%[^a-zA-Z0-9#]", str);
    printf("Test 6 - Parsed items: %d, str: %s\n", result, str);

    strcpy(str, ""); result = my_sscanf("key=value", "%[^=]", str);
    printf("Test 7 - Parsed items: %d, str: %s\n", result, str);

    strcpy(str, ""); result = my_sscanf("\xC4\xE3\xBA\xC3=value", "%[^=]", str);
    printf("Test 8 - Parsed items: %d, str: %s\n", result, str);

    printf("\n=== Additional Normal Tests ===\n");

    int d; result = my_sscanf("123 abc", "%d", &d);
    printf("Test 9 - Parsed items: %d, d: %d\n", result, d);

    unsigned int u; result = my_sscanf("456 def", "%u", &u);
    printf("Test 10 - Parsed items: %d, u: %u\n", result, u);

    unsigned int x; result = my_sscanf("1a2b ghi", "%x", &x);
    printf("Test 11 - Parsed items: %d, x: %x\n", result, x);

    float fval; result = my_sscanf("3.14 jkl", "%f", &fval);
    printf("Test 12 - Parsed items: %d, f: %.2f\n", result, fval);

    strcpy(str, ""); result = my_sscanf("hello world", "%s", str);
    printf("Test 13 - Parsed items: %d, str: %s\n", result, str);

    strcpy(str, ""); result = my_sscanf("abcdef123", "%[a-z]", str);
    printf("Test 14 - Parsed items: %d, str: %s\n", result, str);

    char c; result = my_sscanf("xyz", "%c", &c);
    printf("Test 15 - Parsed items: %d, c: %c\n", result, c);

    int n; result = my_sscanf("abc123", "%*s%n", &n);
    printf("Test 16 - Parsed items: %d, n: %d\n", result, n);

    result = my_sscanf("100% complete", "100%% complete");
    printf("Test 17 - Parsed items: %d (should be 0, no assignment)\n", result);

    strcpy(str, ""); result = my_sscanf("abcdefgh", "%5s", str);
    printf("Test 18 - Parsed items: %d, str: %s\n", result, str);

    int num; float fnum; strcpy(str, "");
    result = my_sscanf("42 3.14 hello", "%d %f %s", &num, &fnum, str);
    printf("Test 19 - Parsed items: %d, num: %d, fnum: %.2f, str: %s\n", result, num, fnum, str);

    printf("\n=== Additional Abnormal Tests ===\n");

    result = my_sscanf("abc", "%d", &d);
    printf("Test 20 - Parsed items: %d (expect 0, invalid int)\n", result);

    result = my_sscanf("xyz", "%f", &fval);
    printf("Test 21 - Parsed items: %d (expect 0, invalid float)\n", result);

    strcpy(str, ""); result = my_sscanf("", "%s", str);
    printf("Test 22 - Parsed items: %d, str: %s (expect 0, empty input)\n", result, str);

    strcpy(str, ""); result = my_sscanf("123", "%[a-z]", str);
    printf("Test 23 - Parsed items: %d, str: %s (expect 0, no match)\n", result, str);

    strcpy(str, ""); result = my_sscanf("aaa", "%[^a]", str);
    printf("Test 24 - Parsed items: %d, str: %s (expect 0, all excluded)\n", result, str);

    strcpy(str, ""); result = my_sscanf("abc", "%[", str);
    printf("Test 25 - Parsed items: %d (expect error)\n", result);

    result = my_sscanf("123", "%z", &d);
    printf("Test 26 - Parsed items: %d (expect error)\n", result);

    result = my_sscanf("42", "%d %f", &num, &fnum);
    printf("Test 27 - Parsed items: %d (expect 1, partial match)\n", result);

    return 0;
}