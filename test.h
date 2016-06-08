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

enum Day
{
    Monday,
    Tuesday,
    Wednesday = 5,
    Thursday
};

enum Blah
{
    A,
    B,
    C = 10,
    D = 20
}

typedef enum Day Day

int sum(int a, int b);
void test_blah(struct blah *a);
void swap(int *a, int *b);
void iterate(int **vector, char ct);
Day get_wednesday();

die(msgerr_test_ignore)

