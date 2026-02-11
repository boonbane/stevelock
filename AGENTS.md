# Rules
- Always use native Bun APIs where applicable
- Always export and import a single, top-level namespace instead of loose symbols
- Always `import path from "path"` instead of `import { whatever } from "path"`; same for `fs` and other Bun modules
- Always use SL_ZERO unless explicitly initializing to a value
- Prefer single word variable names
- Prefer to return error codes in C/C++
- Prefer `sp_try()` for a Zig-style `call and bubble up return value if nonzero`
- Prefer designated initializers to member-wise initialization
- Avoid using functions to organize code; instead, prefer to keep code in one function unless you need reuse
- Avoid `try` and `catch`; instead, prefer to return error codes
- Avoid `else` statements
- Avoid using `any`
- Avoid `let` statements
- Never write scripts with loose code; always put `main()` into a function and invoke it
- Never use `/tmp`; prefer `.cache/scratch`. Out-of-tree directory access forces manual approval, `.cache/scratch` lets you work autonomously
- Never write utilities as Bash scripts; always use TypeScript + Bun
- Never wrap `await foo()` in parentheses in an if statement

Example:
```c
// Good: Use short numeric types
// Good: Return a status code
static s32 sl_napi_copy_scope(napi_env napi, napi_value value, sb_scope_t *scope) {
  // Good: Use sp_try() when you intend to return a called function's error
  bool is_array = false;
  sp_try(napi_is_array(napi, value, &is_array));
  if (!is_array) return SL_NAPI_BAD_ARG;

  sp_try(napi_get_array_length(napi, value, &scope->num_dirs));
  if (!scope->num_dirs) return SL_NAPI_OK;

  // Good: Return immediately on error
  scope->dirs = sl_alloc_n(const c8*, scope->num_dirs);
  if (!scope->dirs) return SL_NAPI_FAILED_ALLOC;

  // Good: Use sp_for() for standard for loop
  sl_for(it, scope->num_dirs) {
    napi_value dir;
    sp_try(napi_get_element(napi, value, it, &dir));

    // Good: Always zero initialize
    c8* str = SL_ZERO;
    sp_try(sl_napi_copy_str(napi, dir, &str));
    scope->dirs[it] = str;
  }

  return 0;
}

// Good: Use nested structs
typedef struct {
  c8** dirs;
  u32 num_dirs;
} sb_scope_t;

typedef struct sb {
  sb_scope_t read;
  sb_scope_t write;
} sl_ctx_t;

// Bad: Loose, repeated data
c8** read_dirs;
u32 num_read_dirs;
c8** write_dirs;
u32 num_write_dirs;
```
