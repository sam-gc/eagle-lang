struct blah
{
    int i;
    int j;
};

typedef struct blah blahb;

typedef struct
{
    int a;
    char b;

} TestStruct;

union test
{
    int a;
    double b;
};

int sum(int a, int b);
void test_blah(struct blah *a);
void swap(int *a, int *b);
void iterate(int **vector, char ct);
