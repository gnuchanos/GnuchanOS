###### import ######################################################
# .gcsf, .gclib

import file, file, file --> single file link
#   file.things --> how to call

import folder in file, file, file --> multi import
#   file.things --> how to call

from file import things, things, things
#   classic python like call things

from folder import file, file, file
#   in folder call

import file as alias
from module import thing as alias
###### import ######################################################

###### 101 #########################################################
string = "very very very long string";
number1 = 10;
number2 = 10.3000;
boolean = True; # False, extra true, false

z = 3 + 4j;
value = None;


# list can do [10, 300.200, 'c', "stringler"];
number1_list = [10, 20, 30, 40, 50, 60];
number2_list = [10.300, 20.300, 30.300, 40.300, 50.300, 60.300];
string_list  = ["half life 3", "Portal 3", "Left 4 Dead"];

number1_tuple = (1000, 3000, 4000, 6000, 7000);
string_tuple = ("a", 'd', "Half Life 3", "Left 4 Dead 3");


numbers = {1, 2, 3, 4};
player = {
    "name": "Gordon",
    "hp": 100,
    "alive": True
}

+ - * / // % **
== != < > <= >=
and or not

= += -= *= /= %=
###### 101 #########################################################

###### type conversion #############################################
i32(v)       # int(v)
f64(v)       # float(v)
str(v)       # str(v)
b8(v)        # bool(v)
ch(v)        # chr(v)

ord("A")     # 65
hex(255)     # "0xff"
bin(10)      # "0b1010"
oct(64)      # "0o100"

list(x)      # list constructor
tuple(x)     # tuple constructor
set(x)       # set constructor
dict(x)      # dict constructor
bytes(x)     # bytes constructor
bytearray(x) # bytearray constructor
###### type conversion #############################################

###### string methods ##############################################
"hello".upper()       # HELLO
"HELLO".lower()       # hello
"  x  ".strip()       # x
"a,b,c".split(",")    # ["a", "b", "c"]
",".join(["a","b"])   # "a,b"
"hello".startswith("h") # True
"hello".endswith("o")   # True
"hello".replace("h","y") # "yello"
"hello".find("l")     # 2
"hello".count("l")    # 2
len("hello")          # 5
"hello"[1:4]          # "ell"  -- slicing
f"value = {x}"        # f-string
"hello".capitalize()  # "Hello"
"hello world".title() # "Hello World"
"HELLO".isupper()     # True
"hello".islower()     # True
"123".isdigit()       # True
"abc".isalpha()       # True
"abc123".isalnum()    # True
"   ".isspace()       # True
"hello".ljust(10)     # "hello     "  (left justify)
"hello".rjust(10)     # "     hello"  (right justify)
"hello".center(10)    # "  hello   "
"hello".zfill(10)     # "00000hello"  (zero pad)
###### string methods ##############################################

###### slicing #####################################################
list[1:4]       # index 1..3
list[:3]        # first 3
list[2:]        # index 2 to end
list[-1]        # last element
list[-3:]       # last 3 elements
list[:-2]       # all except last 2
list[::2]       # every 2nd element
list[::-1]      # reversed copy
list[1:8:3]     # start:stop:step

string[2:5]     # works on strings too
tuple[1:3]      # works on tuples too
bytes[0:4]      # works on bytes too

# slice() object
sl = slice(1, 8, 3);
list[sl];       # same as list[1:8:3]
###### slicing #####################################################

###### unpacking / destructuring ###################################
a, b = (1, 2)          # a=1, b=2
a, b = b, a            # swap
a, *rest = [1,2,3,4]   # a=1, rest=[2,3,4]
first, *mid, last = [1,2,3,4,5]  # first=1, mid=[2,3,4], last=5
a, b, *_, c = range(10)          # skip middle with _

x, y = (10, 20)        # tuple unpacking
for (key, val in dict.items()) {  # dict unpacking in loop
    print(key, val);
}

def fn(a, b, c) { ... }
args = [1, 2, 3];
fn(*args);             # list unpacking in call

kwargs = {"a": 1, "b": 2};
fn(**kwargs);          # dict unpacking in call
###### unpacking / destructuring ###################################

###### f-string format specifiers ##################################
f"{x:.2f}"       # 2 decimal places: 3.14
f"{x:>10}"       # right-aligned in width 10
f"{x:<10}"       # left-aligned
f"{x:^10}"       # center-aligned
f"{x:010}"       # zero-padded: 0000000042
f"{x:+}"         # show sign: +42
f"{x:,}"         # thousands separator: 1,234
f"{x:.0%}"       # percentage: 50%
f"{name!r}"      # repr of name
f"{name!s}"      # str of name (default)
f"{name!a}"      # ascii-only repr
###### f-string format specifiers ##################################

###### list / set / dict methods ###################################
list.append(x)
list.pop()
list.pop(i)
list.remove(x)
list.insert(i, x)
list.sort()
list.reverse()
list.count(x)
list.index(x)
len(list)
list.copy()        # shallow copy
list.clear()       # empty the list
list.extend(iter)  # extend with iterable

set.add(x)
set.remove(x)
set.discard(x)
set.union(other)
set.intersection(other)
set.difference(other)
set.symmetric_difference(other)
set.isdisjoint(other)
set.issubset(other)
set.issuperset(other)
set.copy()
set.clear()

dict.keys()
dict.values()
dict.items()
dict.get(key, default)
dict.update({k:v})
dict.pop(key)
dict.popitem()     # remove and return last (key, val)
dict.setdefault(key, default)
dict.copy()
dict.clear()
del dict[key]      # delete key
###### list / set / dict methods ###################################

###### set / dict operators ########################################
a | b        # union (set)
a & b        # intersection (set)
a - b        # difference (set)
a ^ b        # symmetric difference (set)
a <= b       # issubset
a >= b       # issuperset
a < b        # proper subset
a > b        # proper superset

{**a, **b}   # dict merge
{**a, x: 1}  # dict merge with override
###### set / dict operators ########################################

###### del #########################################################
del x              # delete variable
del list[0]        # delete by index
del list[1:3]      # delete slice
del dict["key"]    # delete dict key
del obj.attr       # delete attribute
###### del #########################################################

###### bitwise #####################################################
a & b     # AND
a | b     # OR
a ^ b     # XOR
~a        # NOT
a << 2    # left shift
a >> 2    # right shift
###### bitwise #####################################################

###### operators: in / is / not in / not is ########################
x in list        # True if x in list
x not in list    # True if x not in list
x is y           # identity check
x is not y       # negated identity
###### operators: in / is / not in / not is ########################

###### if/elif/else ################################################
if (hp > 0) {
    print("alive")
}
elif (hp == 0) {
    print("dead");
}
else {
    print("error");
}

# ternary
result = "alive" if (hp > 0) else "dead";
###### if/elif/else ################################################

###### WHILE ########################################################
while (i < 10) {
    i += 1;
}

while (True) {
    # break to exit
}
###### WHILE ########################################################

###### for ##########################################################
for (x in numbers) {
    print(x);
}

for (i in range(10)) {
    print(i);
}

for (i, val in enumerate(items)) {
    print(i, val);
}

for (a, b in zip(list1, list2)) {
    print(a, b);
}

for (x in reversed(list)) {
    print(x);
}

for (x in sorted(list)) {
    print(x);
}
###### for ##########################################################

###### loop control: break / continue / else / pass #################
for (x in items) {
    if (x == 0) {
        break;       # exit loop
    }
    if (x % 2 == 0) {
        continue;    # skip this iteration
    }
}

# else on loop -- runs if no break
for (x in items) {
    if (x == target) {
        break;
    }
}
else {
    print("not found");
}

pass;  # placeholder / no-op
###### loop control: break / continue / else / pass #################

###### match / case #################################################
match (value) {
    case 1:
        print("one");
    case 2, 3:
        print("two or three");
    case _:
        print("default");
}

# match with guard
match (point) {
    case (x, y) if (x == y):
        print("on diagonal");
    case (x, y):
        print(x, y);
}
###### match / case #################################################

###### comprehensions ###############################################
squares = [x*x for (x in range(10))];
evens   = [x for (x in numbers) if (x % 2 == 0)];
squared_set = {x*x for (x in numbers)};
squared_dict = {x: x*x for (x in numbers)};

# generator expression (lazy)
squares_gen = (x*x for (x in range(10)));
for (val in squares_gen) {
    print(val);
}

# nested comprehensions
flat = [y for (row in matrix) for (y in row)];
###### comprehensions ###############################################

###### def ##########################################################
def add(a, b) {
    return a + b;
}

def say_hello(name = "world") {
    print("hello", name);
}

# type annotations
def add(a: int, b: int) -> int {
    return a + b;
}
###### def ##########################################################

###### *args / **kwargs #############################################
def log(*args) {
    for (arg in args) {
        print(arg);
    }
}

def config(**kwargs) {
    for (key, val in kwargs.items()) {
        print(key, "=", val);
    }
}
###### *args / **kwargs #############################################

###### lambda #######################################################
add = lambda (a, b) { return a + b; };
evens = filter(lambda (x) { return x % 2 == 0; }, numbers);

# sorted with key
sorted(items, key = lambda (x) { return x[1]; });
sorted(items, key = lambda (x) { return len(x); }, reverse = True);
###### lambda #######################################################

###### generators / yield ###########################################
def counter(n) {
    i = 0;
    while (i < n) {
        yield i;
        i += 1;
    }
}

for (x in counter(5)) {
    print(x);
}

# generator expression
squares = (x*x for (x in range(10)));

# yield from (delegate to sub-generator)
def merge(gen1, gen2) {
    yield from gen1;
    yield from gen2;
}
###### generators / yield ###########################################

###### class ########################################################
class Player() {
    def __init__(self) {
        self.hp = 100;
    }
}
###### class ########################################################

###### inheritance ##################################################
class Enemy(Player) {
    def __init__(self) {
        super().__init__();
        self.hp = 50;
    }
}

# isinstance / issubclass
isinstance(obj, Player);     # True / False
issubclass(Enemy, Player);   # True

# __class__ / __dict__
obj.__class__;               # class of obj
obj.__dict__;                # attribute dict
###### inheritance ##################################################

###### __slots__ ####################################################
class Player() {
    __slots__ = ["hp", "name"];
    # prevents creating __dict__, saves memory
    # no new attributes outside __slots__
}
p = Player();
p.hp = 100;      # ok
p.x = 5;         # ERROR: no 'x' in __slots__
###### __slots__ ####################################################

###### magic methods (dunder methods) ###############################
class Vector() {
    def __init__(self, x, y) {
        self.x = x;
        self.y = y;
    }

    def __new__(cls, x, y) {
        # called before __init__ to create instance
        return super().__new__(cls);
    }

    def __str__(self) {
        return f"({self.x}, {self.y})";
    }

    def __repr__(self) {
        return f"Vector({self.x}, {self.y})";
    }

    def __len__(self) {
        return 2;
    }

    def __bool__(self) {
        return (self.x != 0 or self.y != 0);
    }

    def __getitem__(self, i) {
        if (i == 0) return self.x;
        if (i == 1) return self.y;
    }

    def __setitem__(self, i, val) {
        if (i == 0) self.x = val;
        if (i == 1) self.y = val;
    }

    def __contains__(self, item) {
        return (item == self.x or item == self.y);
    }

    def __add__(self, other) {
        return Vector(self.x + other.x, self.y + other.y);
    }

    def __sub__(self, other) {
        return Vector(self.x - other.x, self.y - other.y);
    }

    def __eq__(self, other) {
        return (self.x == other.x and self.y == other.y);
    }

    def __lt__(self, other) {
        return (self.x + self.y) < (other.x + other.y);
    }

    def __iter__(self) {
        # return iterator over x, y
        return iter([self.x, self.y]);
    }

    def __call__(self) {
        print("Vector called!");
    }
}

v = Vector(1, 2);
print(v);          # __str__  → (1, 2)
v[0] = 10;         # __setitem__
print(len(v));     # __len__  → 2
1 in v;            # __contains__ → True
v == Vector(10,2); # __eq__   → True
for (val in v) {   # __iter__
    print(val);
}
v();               # __call__ → "Vector called!"
###### magic methods (dunder methods) ###############################

###### @property setter/deleter #####################################
class Player() {
    def __init__(self) {
        self._hp = 100;
    }

    @property
    def hp(self) {
        return self._hp;
    }

    @hp.setter
    def hp(self, val) {
        if (val < 0) {
            self._hp = 0;
        }
        else {
            self._hp = val;
        }
    }

    @hp.deleter
    def hp(self) {
        del self._hp;
    }
}

p = Player();
p.hp = 50;         # uses setter
print(p.hp);       # uses getter → 50
###### @property setter/deleter #####################################

###### decorators ###################################################
@staticmethod
def helper() { ... }

@classmethod
def factory(cls) { ... }

@property
def health(self) {
    return self.hp;
}

# decorator with arguments
def repeat(count) {
    def decorator(fn) {
        def wrapper(*args) {
            for (i in range(count)) {
                fn(*args);
            }
        }
        return wrapper;
    }
    return decorator;
}

@repeat(3)
def greet(name) {
    print("hello", name);
}

greet("world");  # prints 3 times

# @abstractmethod (from abc)
from abc import ABC, abstractmethod;

class Animal(ABC) {
    @abstractmethod
    def make_sound(self) {
        pass;  # must be overridden
    }
}

# @contextmanager (from contextlib)
from contextlib import contextmanager;

@contextmanager
def open_file(name, mode) {
    file = open(name, mode);
    try {
        yield file;
    }
    finally {
        file.close();
    }
}

with open_file("data.txt", "r") as f {
    data = f.read();
}
###### decorators ###################################################

###### @dataclass ###################################################
@dataclass
class Point {
    x: int;
    y: int;
}

@dataclass
class Player {
    name: str;
    hp: int = 100;
    alive: bool = True;
}

p = Player("Gordon");  # no __init__ to write
print(p.name);         # Gordon
print(p.hp);           # 100
###### @dataclass ###################################################

###### global / nonlocal ############################################
x = 10;

def outer() {
    x = 20;          # local x

    def inner() {
        nonlocal x;  # refers to outer's x
        x = 30;
    }

    inner();
    print(x);        # 30
}

def change_global() {
    global x;
    x = 99;
}
###### global / nonlocal ############################################

###### __all__ ######################################################
# in module file: controls what `from module import *` exports
__all__ = ["public_func", "PublicClass"];
# names not in __all__ stay private
###### __all__ ######################################################

###### try/except/finally ###########################################
try {

}
except {

}
finally {

}

# with type
try {
    x = 1 / 0;
}
except ZeroDivisionError {
    print("can't divide by zero");
}

# with else clause (runs if no exception)
try {
    x = int(value);
}
except ValueError {
    print("bad value");
}
else {
    print("ok:", x);
}

# except with exception variable
try {
    x = 1 / 0;
}
except ZeroDivisionError as e {
    print(e);        # "division by zero"
}
###### try/except/finally ###########################################

###### raise / assert ###############################################
raise ValueError("invalid state");
assert x > 0, "x must be positive";

# re-raise
try {
    x = 1 / 0;
}
except {
    print("error");
    raise;           # re-raise same exception
}
###### raise / assert ###############################################

###### file i/o #####################################################
file = open("data.txt", "r");
data = file.read();
file.close();

# with statement (auto close)
with open("data.txt", "w") as file {
    file.write("hello");
}

# modes: r, w, a, rb, wb, r+, w+, a+

# read helpers
data = file.read();         # entire file
data = file.read(n);        # n bytes
line = file.readline();     # one line
lines = file.readlines();   # all lines

# write helpers
file.write("text");
file.writelines(list_of_strings);

# position
pos = file.tell();          # current position
file.seek(0);               # go to start
file.seek(0, 2);            # go to end
file.seek(10);              # go to byte 10
###### file i/o #####################################################

###### built-in functions ###########################################
len(x)       # length
type(x)      # type
print(...)   # output
input(...)   # input from user
range(n)     # 0..n-1
enumerate(x) # (index, value) pairs
zip(a, b)    # paired tuples
map(f, x)    # apply f to each
filter(f, x) # keep where f true
sorted(x)    # sorted copy
reversed(x)  # reversed iterator
sum(x)       # total
min(x)       # minimum
max(x)       # maximum
abs(x)       # absolute value
round(x, n)  # round to n digits
any(x)       # True if any element is truthy
all(x)       # True if all elements are truthy
callable(x)  # True if x is callable
dir(x)       # list attributes of x
vars(x)      # dict of x's attributes
id(x)        # unique integer id of x
hash(x)      # hash value of x (for set/dict)
help(x)      # show documentation for x
repr(x)      # string representation (like __repr__)
pow(x, y)    # x ** y
pow(x, y, z) # (x ** y) % z   (modular exponent)
divmod(a, b) # (a // b, a % b) as tuple
oct(x)       # octal string    "0o144"
chr(i)       # char from int   chr(65) → "A"
bytes(x)     # immutable bytes
bytearray(x) # mutable bytes
sorted(x, key=lambda (e) { return e[1]; }, reverse=True)
###### built-in functions ###########################################

###### __name__ == "__main__" #######################################
if (__name__ == "__main__") {
    main();
}
###### __name__ == "__main__" #######################################

######################################################################
# STANDARD LIBRARY MODULES
######################################################################

###### os — operating system ########################################
import os;

os.name             # nt / posix
os.getcwd()         # current working directory
os.chdir("path")    # change directory
os.listdir(".")     # list directory contents

os.mkdir("dir")     # create directory
os.makedirs("a/b/c") # create nested directories
os.rmdir("dir")     # remove empty directory
os.removedirs("a/b/c") # remove nested empty dirs
os.remove("file")   # delete file
os.rename("old", "new") # rename file
os.replace("old", "new") # rename (overwrite if exists)

os.path.exists("path")  # True if path exists
os.path.isfile("path")  # True if file
os.path.isdir("path")   # True if directory
os.path.join("a", "b")  # "a/b" (platform separator)
os.path.basename("/a/b/c.txt") # "c.txt"
os.path.dirname("/a/b/c.txt")  # "/a/b"
os.path.split("/a/b/c.txt")    # ("/a/b", "c.txt")
os.path.splitext("file.txt")   # ("file", ".txt")
os.path.abspath("rel")   # absolute path
os.path.getsize("file")  # file size in bytes
os.path.getmtime("file") # last modified timestamp

os.environ["PATH"]   # environment variable
os.environ.get("HOME", "default")
os.putenv("KEY", "val")

os.system("command") # run shell command (blocking)
os.getpid()          # current process ID
os.cpu_count()       # number of CPUs
###### os — operating system ########################################

###### sys — system / interpreter ###################################
import sys;

sys.argv             # command line arguments list
sys.argv[0]          # script name
sys.exit(0)          # exit with code
sys.exit("message")  # exit with error message
sys.path             # module search paths (list)
sys.platform         # "win32", "linux", "darwin"
sys.version          # Python version string
sys.stdin            # standard input file
sys.stdout           # standard output file
sys.stderr           # standard error file
sys.modules          # dict of loaded modules
sys.getsizeof(obj)   # memory size of object
###### sys — system / interpreter ###################################

###### argparse — command line argument parsing #####################
import argparse;

parser = argparse.ArgumentParser(description="my tool");
parser.add_argument("name");               # positional arg
parser.add_argument("-v", "--verbose", action="store_true");
parser.add_argument("-o", "--output", default="out.txt");
parser.add_argument("--count", type=int, default=1);

args = parser.parse_args();
args.name;          # positional
args.verbose;       # True/False
args.output;        # "out.txt"
args.count;         # 1
###### argparse — command line argument parsing #####################

###### glob — file pattern matching #################################
import glob;

glob.glob("*.txt")           # list all .txt in current dir
glob.glob("**/*.py")         # recursive (Python 3.5+)
glob.glob("src/**/*.c", recursive=True)
###### glob — file pattern matching #################################

###### json — JSON encoding/decoding ################################
import json;

data = {"name": "Gordon", "hp": 100};
json_str = json.dumps(data);         # dict → JSON string
json_str = json.dumps(data, indent=2); # pretty print

parsed = json.loads(json_str);       # JSON string → dict/value

with open("data.json", "w") as f {
    json.dump(data, f);              # dict → JSON file
}

with open("data.json", "r") as f {
    data = json.load(f);             # JSON file → dict
}
###### json — JSON encoding/decoding ################################

###### csv — CSV file reading/writing ################################
import csv;

# reading
with open("data.csv", "r") as f {
    reader = csv.reader(f);
    for (row in reader) {
        print(row);    # list of strings
    }
}

# writing
with open("data.csv", "w") as f {
    writer = csv.writer(f);
    writer.writerow(["name", "hp", "alive"]);
    writer.writerow(["Gordon", 100, True]);
}

# dict reader/writer
with open("data.csv", "r") as f {
    reader = csv.DictReader(f);
    for (row in reader) {
        print(row["name"], row["hp"]);
    }
}

with open("data.csv", "w") as f {
    fieldnames = ["name", "hp"];
    writer = csv.DictWriter(f, fieldnames=fieldnames);
    writer.writeheader();
    writer.writerow({"name": "Gordon", "hp": 100});
}
###### csv — CSV file reading/writing ################################

###### re — regular expressions #####################################
import re;

re.match(pattern, string)   # match at start, returns Match or None
re.search(pattern, string)  # search anywhere, returns Match or None
re.findall(pattern, string) # list of all matches
re.finditer(pattern, string) # iterator of Match objects
re.sub(pattern, repl, string) # replace all matches
re.split(pattern, string)   # split on pattern

# common patterns
r"\d+"       # one or more digits
r"\w+"       # one or more word chars
r"\s+"       # one or more whitespace
r"[a-zA-Z]"  # letters
r"^start"    # begins with
r"end$"      # ends with

# flags
re.IGNORECASE  # ignore case
re.MULTILINE   # ^ and $ match lines
re.DOTALL      # . matches newline

# match object
m = re.search(r"(\d+)", "abc123def");
if (m) {
    print(m.group(0));  # "123"
    print(m.group(1));  # "123"
    print(m.start());   # 3
    print(m.end());     # 6
}
###### re — regular expressions #####################################

###### math — mathematics ###########################################
import math;

math.pi         # 3.141592653589793
math.e          # 2.718281828459045
math.inf        # infinity
math.nan        # not a number

math.sqrt(x)    # square root
math.pow(x, y)  # x ** y
math.exp(x)     # e ** x
math.log(x)     # natural log
math.log10(x)   # base-10 log
math.log2(x)    # base-2 log

math.sin(x)     # sine (radians)
math.cos(x)     # cosine
math.tan(x)     # tangent
math.asin(x)    # arc sine
math.acos(x)    # arc cosine
math.atan(x)    # arc tangent
math.atan2(y, x) # arctan(y/x)

math.degrees(x) # rad → deg
math.radians(x) # deg → rad

math.ceil(x)    # round up
math.floor(x)   # round down
math.trunc(x)   # truncate toward zero
math.fabs(x)    # absolute value (float)
math.factorial(n) # n!
math.gcd(a, b)  # greatest common divisor
math.isfinite(x) # True if finite
math.isinf(x)   # True if infinite
math.isnan(x)   # True if NaN
###### math — mathematics ###########################################

###### random — random numbers ######################################
import random;

random.random()         # float in [0.0, 1.0)
random.randint(1, 10)   # int in [1, 10] inclusive
random.randrange(0, 10, 2) # int from range(start, stop, step)
random.uniform(1.0, 5.0) # float in [1.0, 5.0)
random.choice(items)    # random element from non-empty list
random.choices(items, k=3) # k random elements (with replacement)
random.sample(items, k=3) # k unique random elements
random.shuffle(items)   # shuffle list in place
random.seed(42)         # set seed for reproducibility
random.getstate()       # get internal state
random.setstate(state)  # restore internal state
###### random — random numbers ######################################

###### statistics — basic statistics ################################
import statistics;

statistics.mean(data)      # arithmetic mean
statistics.median(data)    # median (middle value)
statistics.mode(data)      # most common value
statistics.stdev(data)     # sample standard deviation
statistics.variance(data)  # sample variance
statistics.quantiles(data) # quartiles (4 groups)
###### statistics — basic statistics ################################

###### datetime — date and time #####################################
import datetime;

# date
d = datetime.date(2026, 7, 13);   # date object
today = datetime.date.today();    # current date
d.year; d.month; d.day;
d.isoformat();           # "2026-07-13"
d.strftime("%Y-%m-%d");  # format string

# time
t = datetime.time(14, 30, 0);     # time object
t.hour; t.minute; t.second;

# datetime
dt = datetime.datetime(2026, 7, 13, 14, 30);
now = datetime.datetime.now();    # current datetime
utc = datetime.datetime.utcnow(); # UTC datetime
dt.year; dt.month; dt.day; dt.hour; dt.minute; dt.second;
dt.isoformat();        # "2026-07-13T14:30:00"
dt.strftime("%d/%m/%Y %H:%M");    # "13/07/2026 14:30"

# timedelta
delta = datetime.timedelta(days=7, hours=3);
future = now + delta;
past = now - datetime.timedelta(days=1);
delta.days; delta.seconds; delta.total_seconds();

# parsing string → datetime
parsed = datetime.datetime.strptime("2026-07-13", "%Y-%m-%d");
###### datetime — date and time #####################################

###### time — time functions ########################################
import time;

time.sleep(seconds);    # pause execution
t = time.time();        # current timestamp (seconds since epoch)
time.ctime(t);          # "Mon Jul 13 14:30:00 2026"
time.gmtime(t);         # UTC struct_time
time.localtime(t);      # local struct_time
time.mktime(time.localtime()); # struct_time → timestamp

# struct_time fields
t = time.localtime();
t.tm_year; t.tm_mon; t.tm_mday;
t.tm_hour; t.tm_min; t.tm_sec;
t.tm_wday; t.tm_yday;
###### time — time functions ########################################

###### threading — concurrent execution #############################
import threading;

# thread
def worker(name) {
    print("hello from", name);
}

t = threading.Thread(target=worker, args=("Thread-1",));
t.start();              # start thread
t.join();               # wait for thread to finish
t.daemon = True;        # thread exits when main exits

# lock
lock = threading.Lock();
lock.acquire();
# critical section
lock.release();

# with lock (auto acquire/release)
with lock {
    # critical section
}

# threading.current_thread().name
# threading.active_count()
# threading.enumerate()  # list of active threads
###### threading — concurrent execution #############################

###### socket — networking ##########################################
import socket;

# TCP client
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
sock.connect(("example.com", 80));
sock.send(b"GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
data = sock.recv(4096);
sock.close();

# UDP
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM);
sock.sendto(b"hello", ("127.0.0.1", 12345));
data, addr = sock.recvfrom(4096);

# get host info
info = socket.gethostbyname("example.com");
hostname = socket.gethostname();
###### socket — networking ##########################################

###### sqlite3 — embedded database ##################################
import sqlite3;

conn = sqlite3.connect("data.db");   # file or ":memory:"
c = conn.cursor();

c.execute("CREATE TABLE IF NOT EXISTS players (name TEXT, hp INT)");
c.execute("INSERT INTO players VALUES (?, ?)", ("Gordon", 100));
conn.commit();

c.execute("SELECT * FROM players");
rows = c.fetchall();      # list of tuples
for (row in rows) {
    print(row);
}

conn.close();
###### sqlite3 — embedded database ##################################

###### tempfile — temporary files ###################################
import tempfile;

# temporary file (auto-deleted on close)
f = tempfile.TemporaryFile();
f.write(b"data");
f.seek(0);
f.read();

# temporary directory (auto-deleted)
with tempfile.TemporaryDirectory() as tmpdir {
    # tmpdir is a string path
}

# named temporary file
f = tempfile.NamedTemporaryFile(delete=False);
print(f.name);   # system-generated path
f.write(b"data");
f.close();
os.remove(f.name);
###### tempfile — temporary files ###################################

###### copy — shallow/deep copy #####################################
import copy;

original = [1, [2, 3], 4];
shallow = copy.copy(original);      # shallow copy
deep = copy.deepcopy(original);     # deep copy (recursive)
# shallow: inner list is still shared
# deep:   everything is independent
###### copy — shallow/deep copy #####################################

###### pprint — pretty printing #####################################
import pprint;

data = {"name": "Gordon", "items": ["gun", "crowbar"], "stats": {"hp": 100, "armor": 50}};
pprint.pprint(data);   # formatted output
pprint.pprint(data, indent=2, width=40);

# as string
formatted = pprint.pformat(data);
###### pprint — pretty printing #####################################

###### struct — binary data packing #################################
import struct;

# pack: Python values → bytes
packed = struct.pack("i4sf", 42, b"test", 3.14);  # int, bytes[4], float
# unpack: bytes → Python values
values = struct.unpack("i4sf", packed);

# format chars: i=int, f=float, d=double, s=bytes, c=char, H=u16, B=u8
# ">i"  = big-endian int
# "<if" = little-endian int + float
###### struct — binary data packing #################################

###### pickle — object serialization ################################
import pickle;

# serialize
data = {"name": "Gordon", "hp": 100, "items": ["crowbar"]};
bytes_data = pickle.dumps(data);    # object → bytes

# deserialize
obj = pickle.loads(bytes_data);     # bytes → object

# file I/O
with open("data.pkl", "wb") as f {
    pickle.dump(data, f);
}
with open("data.pkl", "rb") as f {
    data = pickle.load(f);
}
###### pickle — object serialization ################################

###### base64 — base64 encoding #####################################
import base64;

encoded = base64.b64encode(b"hello world");   # bytes → base64 bytes
decoded = base64.b64decode(encoded);          # base64 bytes → bytes

# URL-safe variant
url_safe = base64.urlsafe_b64encode(b"hello");
###### base64 — base64 encoding #####################################

###### uuid — unique identifiers ####################################
import uuid;

u = uuid.uuid4();         # random UUID (most common)
u.hex;                    # hex string (no dashes)
str(u);                   # "550e8400-e29b-41d4-a716-446655440000"

uuid.uuid1();             # host + time based
uuid.uuid5(uuid.NAMESPACE_DNS, "example.com");  # name-based (SHA-1)
###### uuid — unique identifiers ####################################

###### zipfile — ZIP archives #######################################
import zipfile;

# reading
with zipfile.ZipFile("archive.zip", "r") as z {
    names = z.namelist();           # list of file names
    info = z.getinfo("file.txt");
    data = z.read("file.txt");      # read file content
    z.extract("file.txt", "out/");  # extract to dir
    z.extractall("out/");           # extract all
}

# writing
with zipfile.ZipFile("archive.zip", "w") as z {
    z.write("file.txt");            # add file to archive
    z.write("dir/file2.txt");
    z.writestr("data.txt", "hello"); # add string as file
}
###### zipfile — ZIP archives #######################################

###### configparser — INI config files ##############################
import configparser;

# reading
config = configparser.ConfigParser();
config.read("config.ini");
value = config["section"]["key"];
value = config.get("section", "key", fallback="default");

# writing
config["server"] = {"host": "localhost", "port": "8080"};
config["user"] = {"name": "admin", "password": "secret"};
with open("config.ini", "w") as f {
    config.write(f);
}
###### configparser — INI config files ##############################

###### textwrap — text formatting ###################################
import textwrap;

text = "very long line that needs to be wrapped";
wrapped = textwrap.wrap(text, width=30);   # list of lines
filled = textwrap.fill(text, width=30);    # single string with \n
dedented = textwrap.dedent("    indented text");
shortened = textwrap.shorten(text, width=20, placeholder="...");
###### textwrap — text formatting ###################################

###### decimal — precise decimal arithmetic #########################
from decimal import Decimal, getcontext;

getcontext().prec = 28;    # set precision (28 digits)

a = Decimal("0.1");
b = Decimal("0.2");
c = a + b;                 # 0.3 (exact, unlike float)
d = Decimal(10) / Decimal(3);  # 3.333333333333333333333333333

# rounding modes: ROUND_HALF_UP, ROUND_DOWN, ROUND_UP
from decimal import ROUND_HALF_UP;
e = Decimal("2.675").quantize(Decimal("0.01"), rounding=ROUND_HALF_UP);
###### decimal — precise decimal arithmetic #########################

###### fractions — rational numbers #################################
from fractions import Fraction;

Fraction(1, 3) + Fraction(1, 3)     # Fraction(2, 3)
Fraction("0.75")                     # Fraction(3, 4)
f = Fraction(10, 4);                # Fraction(5, 2)  (auto-reduced)
f.numerator; f.denominator;         # 5, 2
float(f);                            # 2.5
###### fractions — rational numbers #################################

###### operator — functional operators ##############################
import operator;

operator.add(a, b)        # a + b
operator.sub(a, b)        # a - b
operator.mul(a, b)        # a * b
operator.truediv(a, b)    # a / b
operator.floordiv(a, b)   # a // b
operator.mod(a, b)        # a % b
operator.pow(a, b)        # a ** b
operator.eq(a, b)         # a == b
operator.lt(a, b)         # a < b
operator.gt(a, b)         # a > b
operator.itemgetter(i)    # lambda x: x[i]
operator.attrgetter("attr") # lambda x: x.attr

# sort with operator
sorted(items, key=operator.itemgetter(1));
sorted(items, key=operator.attrgetter("hp"));
###### operator — functional operators ##############################

###### collections — specialized containers #########################
from collections import defaultdict, Counter, namedtuple, deque, OrderedDict;

# defaultdict — dict with default factory
d = defaultdict(int);
d["key"] += 1;    # no KeyError, defaults to 0

d2 = defaultdict(list);
d2["group"].append("item");  # auto-creates empty list

# Counter — count hashable items
c = Counter(["a", "b", "a", "c", "b", "a"]);
c["a"];          # 3
c.most_common(2) # [("a", 3), ("b", 2)]

# namedtuple — lightweight struct
Point = namedtuple("Point", ["x", "y"]);
p = Point(10, 20);
p.x; p.y;        # field access
p[0]; p[1];      # index access

# deque — double-ended queue
dq = deque();
dq.append(x);     # right side
dq.appendleft(x); # left side
dq.pop();         # remove from right
dq.popleft();     # remove from left

# OrderedDict — dict that maintains insertion order
od = OrderedDict();
od["a"] = 1;
od["b"] = 2;
od.move_to_end("a");   # move "a" to end
###### collections — specialized containers #########################

###### itertools — iterator tools ###################################
import itertools;

itertools.count(start=0, step=1)     # infinite counter
itertools.cycle(iterable)            # infinite cycle
itertools.repeat(x, n)              # repeat x n times

itertools.chain(a, b, c)            # concatenate iterables
itertools.chain.from_iterable(iter_of_iters)

itertools.islice(iter, start, stop, step) # slice iterator (lazy)
itertools.takewhile(pred, iter)     # take while predicate True
itertools.dropwhile(pred, iter)     # drop while predicate True

itertools.product([1,2], ["a","b"])  # Cartesian product
itertools.permutations(items, r)     # permutations
itertools.combinations(items, r)     # combinations
itertools.combinations_with_replacement(items, r)

itertools.groupby(iter, key_func)    # group adjacent elements
# (must be sorted by key first)
for (key, group in itertools.groupby(items, key_func)) {
    print(key, list(group));
}
###### itertools — iterator tools ###################################

###### functools — higher-order functions ###########################
import functools;

# partial — freeze arguments
def add(a, b) { return a + b; }
add5 = functools.partial(add, 5);
add5(3);  # 8

# lru_cache — memoization
@functools.lru_cache(maxsize=128)
def fib(n) {
    if (n < 2) return n;
    return fib(n-1) + fib(n-2);
}

# reduce — accumulate
total = functools.reduce(lambda (a, b) { return a + b; }, [1,2,3,4]);
# 10

# @wraps — preserve metadata in decorators
def my_decorator(fn) {
    @functools.wraps(fn)
    def wrapper(*args) {
        return fn(*args);
    }
    return wrapper;
}
###### functools — higher-order functions ###########################

###### pathlib — OO path handling ###################################
from pathlib import Path;

p = Path("/a/b/c.txt");
p.name;        # "c.txt"
p.stem;        # "c"
p.suffix;      # ".txt"
p.parent;      # /a/b
p.parents[0];  # /a/b
p.parents[1];  # /a

Path.cwd()                      # current working directory
Path.home()                     # user home directory

p.exists()
p.is_file()
p.is_dir()
p.stat().st_size
p.stat().st_mtime

p.read_text()                   # read entire file
p.write_text("hello")           # write string
p.read_bytes()                  # read binary
p.write_bytes(b"data")          # write binary

Path(".").iterdir()             # iterate directory entries
Path(".").glob("*.txt")         # glob pattern
Path(".").rglob("**/*.py")      # recursive glob

p.mkdir()                       # create directory
p.mkdir(parents=True)           # create nested dirs
p.rmdir()                       # remove empty dir
p.unlink()                      # delete file
p.rename("new.txt")             # rename

# path join with /
Path("dir") / "sub" / "file.txt"  # "dir/sub/file.txt"
###### pathlib — OO path handling ###################################

###### shutil — high-level file operations ##########################
import shutil;

shutil.copy("src", "dst")        # copy file
shutil.copy2("src", "dst")       # copy with metadata
shutil.copytree("src", "dst")    # copy entire directory tree
shutil.move("src", "dst")        # move file or directory
shutil.rmtree("dir")             # delete entire directory tree
shutil.disk_usage("/")           # total, used, free
shutil.make_archive("name", "zip", "dir") # create zip/tar
shutil.unpack_archive("file.zip", "dir")  # extract archive
###### shutil — high-level file operations ##########################

###### subprocess — run external commands ###########################
import subprocess;

result = subprocess.run(["ls", "-l"]);
result = subprocess.run(["ls", "-l"], capture_output=True);
result.stdout;      # captured stdout (bytes)
result.stderr;      # captured stderr (bytes)
result.returncode;  # exit code (0 = success)
result.check_returncode();  # raise if non-zero

# with text mode
result = subprocess.run(["echo", "hello"], capture_output=True, text=True);
result.stdout;      # "hello\n" (string)

# shell=True (caution: security risk)
subprocess.run("ls -l", shell=True);

# Popen — fine-grained control
proc = subprocess.Popen(["ping", "8.8.8.8"], stdout=subprocess.PIPE);
stdout, stderr = proc.communicate();
proc.wait();

# timeout
result = subprocess.run(["sleep", "10"], timeout=5);  # raises TimeoutExpired
###### subprocess — run external commands ###########################

###### hashlib — cryptographic hashing ##############################
import hashlib;

hashlib.md5(b"data").hexdigest()
hashlib.sha1(b"data").hexdigest()
hashlib.sha256(b"data").hexdigest()

# large data
h = hashlib.sha256();
h.update(b"part1");
h.update(b"part2");
h.hexdigest();
###### hashlib — cryptographic hashing ##############################

###### logging — logging system #####################################
import logging;

logging.basicConfig(level=logging.INFO);
logging.debug("debug message");      # not shown at INFO level
logging.info("info message");
logging.warning("warning message");
logging.error("error message");
logging.critical("critical message");

# with format
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s"
);
###### logging — logging system #####################################

###### contextlib — context utilities ###############################
from contextlib import contextmanager, closing, suppress;

# @contextmanager (see decorators section)
# closing — call close() on exit
with closing(open("file.txt")) as f {
    data = f.read();
}

# suppress — ignore specific exceptions
with suppress(FileNotFoundError) {
    os.remove("missing.txt");  # no error
}
###### contextlib — context utilities ###############################

###### enum — enumerated constants ##################################
from enum import Enum, auto;

class Color(Enum) {
    RED = 1;
    GREEN = 2;
    BLUE = 3;
}

class Direction(Enum) {
    UP = auto();   # auto assigns 1, 2, 3...
    DOWN = auto();
    LEFT = auto();
    RIGHT = auto();
}

Color.RED;           # Color.RED
Color.RED.name;      # "RED"
Color.RED.value;     # 1

for (c in Color) {
    print(c.name, c.value);
}
###### enum — enumerated constants ##################################

###### webbrowser — web browser control #############################
import webbrowser;

webbrowser.open("https://example.com");     # open in default browser
webbrowser.open_new("https://example.com"); # new window
webbrowser.open_new_tab("https://example.com"); # new tab
###### webbrowser — web browser control #############################

###### platform — system/platform info ##############################
import platform;

platform.system();      # "Windows", "Linux", "Darwin"
platform.release();     # "10", "5.4.0-26-generic"
platform.machine();     # "AMD64", "x86_64"
platform.processor();   # "Intel64 Family 6..."
platform.python_version(); # "3.10.0"
platform.node();        # hostname
platform.uname();       # full uname tuple
###### platform — system/platform info ##############################
