# DIZI TIPLERI — ARRAY TYPES
# C99 / gnu99 ornekleri — gcLang temeli icin

## static (sabit boyutlu)
```c
int arr[10];
int arr[5] = {1, 2, 3, 4, 5};
int arr[5] = {0};                         // hepsi 0
int arr[]  = {1, 2, 3};                   // boyut cikarilir → 3
char str[6] = "hello";
char str[] = "hello";                     // otomatik boyut → 6 (null ile)
```

## const
```c
const int arr[3] = {10, 20, 30};
const char* str = "read only";
```

## C99 — Variable Length Array (VLA)
```c
int n = 5;
int vla[n];                               // runtime boyutu
for (int i = 0; i < n; i++) vla[i] = i;

void vla_func(int n, int arr[n]);         // fonksiyon parametresi
void vla_func(int n, int arr[*]);         // * ile (prototype scope)

int m = 3, k = 4;
int matrix[m][k];                         // 2D VLA
```

## C99 — designated initializers
```c
int arr[5] = {[2] = 99};                  // {0, 0, 99, 0, 0}
int arr[5] = {[1] = 10, [4] = 40};

int range[3] = {[0] = 1, [1] = 2, [2] = 3};
```

## C99 — compound literal (array)
```c
int* p = (int[]){1, 2, 3, 4, 5};

void func(int* arr, int n);
func((int[]){10, 20, 30}, 3);             // gecici dizi
```

## C99 — restrict pointer (dizi iliskisi)
```c
void add(int n, int* restrict a, int* restrict b, int* restrict c) {
    for (int i = 0; i < n; i++)
        c[i] = a[i] + b[i];
}
```

## C99 — flexible array member (struct icinde)
```c
struct flex {
    int len;
    int data[];                           // flexible array member
};

struct flex* f = malloc(sizeof(struct flex) + 10 * sizeof(int));
f->len = 10;
```

## gnu99 — zero-length array (GNU extension)
```c
// GNU extension — C99 oncesi flexible array
struct gnu_flex {
    int len;
    int data[0];                          // zero-length array
};

struct gnu_flex* gf = malloc(sizeof(struct gnu_flex) + 10 * sizeof(int));
```

## gnu99 — range initializer (GNU extension)
```c
int arr[10] = {[0 ... 4] = 1, [5 ... 9] = 2};
// 1,1,1,1,1,2,2,2,2,2
```

## gnu99 — typeof ile dizi
```c
int arr[5] = {1, 2, 3, 4, 5};
typeof(arr[0]) x = 10;                    // int x = 10

typeof(arr) copy;                         // int copy[5]
memcpy(copy, arr, sizeof(arr));
```

## gnu99 — statement expression + compound literal
```c
#define arr_sum(n)                                     \
    ({ int _s = 0;                                      \
       for (int _i = 0; _i < (n); _i++)                 \
           _s += ((int[]){1, 2, 3, 4, 5})[_i];          \
       _s; })

int total = arr_sum(5);                                 // 15
```

## cok boyutlu
```c
int mat[2][3] = {{1,2,3},{4,5,6}};
int mat[2][3] = {1,2,3,4,5,6};                         // ayni
int mat[][3] = {{1,2,3},{4,5,6}};                      // boyut cikarilir

for (int i = 0; i < 2; i++)
    for (int j = 0; j < 3; j++)
        mat[i][j] = i + j;
```

## string dizisi
```c
char* names[] = {"ali", "veli", "deli"};
char names[][10] = {"ali", "veli", "deli"};            // static

for (int i = 0; i < 3; i++)
    puts(names[i]);
```

## sizeof ile dizi boyutu
```c
int arr[10];
size_t n = sizeof(arr) / sizeof(arr[0]);                // 10
