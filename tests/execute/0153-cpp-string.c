#define M1(x) "This is a string $ or # or ## " ## #x
#define STR "This is a string $ or # or ## and it is ok!"

int
main(void)
{
        char *s, *t = M1(and it is ok!);

	for (s = STR; *s && *s == *t; ++s)
		++t;

        return *s;
}
