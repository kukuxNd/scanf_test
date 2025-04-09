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