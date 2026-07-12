@lib <stdio>

printf("...", ...);

%d → signed decimal integer
%i → signed integer
%u → unsigned decimal integer
%o → unsigned octal
%x → unsigned hexadecimal (abcdef)
%X → unsigned hexadecimal (ABCDEF)
%f → floating point decimal
%F → floating point decimal
%e → scientific notation (e)
%E → scientific notation (E)
%g → %f veya %e kısa olan
%G → %F veya %E kısa olan
%a → hexadecimal floating point
%A → hexadecimal floating point
%c → character
%s → string
%p → pointer address
%n → o ana kadar yazılan karakter sayısını verilen int pointer’a yazar
%% → % karakterinin kendisi
length modifier ile kullanılanlar
%hd → short
%hu → unsigned short
%hhd → signed char
%hhu → unsigned char
%ld → long
%lu → unsigned long
%lld → long long
%llu → unsigned long long
%zd / %zu → size_t / ilgili size tipi
%td → ptrdiff_t
%jd / %ju → intmax_t / uintmax_t
%lf → double
%Lf → long double
%Le, %Lg, %La → long double versiyonları