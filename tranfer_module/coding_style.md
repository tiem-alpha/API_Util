
- Reduce bugs in embedded systems  
- Improve readability  
- Improve maintainability  
- Improve portability  
- Reliability > Readability > Performance > Convenience  
- Code must be easily checked by compiler/tools  

## 1. GENERAL RULES
- **C Standard**: Use C99, limit compiler extensions, never redefine keywords with `#define`  
- **Line Width**: Max 80 characters per line  
- **Braces**: Always use `{}` for `if, else, for, while, do, switch`  
- **Parentheses**: Always use `()` for clarity, don’t rely on precedence  
- **Abbreviation**: Avoid unclear abbreviations, maintain a shared acronym table  
- **Cast**: Each cast must have a comment explaining why  
- **Keywords to avoid**:  `auto`, `register`;  limit `goto`, `continue`  
- **Keywords to use**:  `static`, `const`, `volatile`  

## 2. COMMENT RULES
- **Format**: Use `//` or `/* */`,  don’t comment-out code with `#if 0 ... #endif`  
- **Content**: Clear, grammatically correct, placed before code blocks, don’t explain the obvious  
- **Markers**:  
  - `WARNING:` risk involved  
  - `NOTE:` explain WHY  
  - `TODO:` pending work  

## 3. WHITE SPACE RULES
- **Spaces**: Add spaces around operators (`a = b + c;`), no spaces in `arr[i]` or `ptr->member`  
- **Alignment**: Align variables, structs, assignments  
- **Blank lines**: One statement per line, blank lines between blocks  
- **Indentation**: 4 spaces  
- **Tabs**:  No tabs  
- **Non-printing**: Use LF  

## 4. MODULE RULES
- **File naming**: lowercase + underscore, `.c` and `.h` must match  
- **Header file**: Include guard, no global variables, only prototypes/typedefs/macros  
- **Source file order**: comment → include → define/typedef → static variable → function prototype → public function → private function  
- **Template**: Use standard template for all files  

## 5. DATA TYPES
- **Naming**: Use `xxx_t` format  
- **Fixed-width types**: should `uint8_t, uint16_t, uint32_t`; not `short, long`  
- **Signed vs Unsigned**:  don’t mix,  don’t use bitwise with signed  
- **Floating point**: Avoid if possible, don’t compare with `==`  
- **Struct**: Check padding and `sizeof`  
- **Boolean**: `bool flag = (x != 0);`  

## 6. PROCEDURE RULES
- **Naming**: lowercase, underscore, meaningful; public functions prefixed with module (`sensor_read()`, `led_is_on()`)  
- **Function**: ≤ 100 lines, prefer single return, private functions = `static`  
- **Macro**: avoid macro functions, use inline  
- **Thread**: suffix `_thread`, `_task`  
- **ISR**: suffix `_isr`, must be fast, non-blocking, use `volatile` for shared data  

## 7. VARIABLE RULES
- **Naming**: TypePrefix (`g_`, `p_`, `pp_`, `b_`, `h_`), lowercase, ≥ 3 characters  
- **Initialization**: All variables must be initialized, pointers → `NULL`, declare close to usage  

## 8. STATEMENT RULES
- **Declaration**: don’t declare multiple variables in one line  
- **if/else**: ≤ 2 levels, don’t assign in condition, always include `else`  
- **switch**: always include `default`, always include `break`  
- **loop**: no magic numbers, infinite loop: `for (;;)`  
- **jump**:  don’t use `abort`, `exit`, `longjmp`  
- **Comparison**: `if (NULL == p_ptr)`  
