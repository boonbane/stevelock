
# Designing declarative N-API object parser proposal

**Session ID:** ses_39b2856fdffeo3wujgmCNpYD7V
**Created:** 2/16/2026, 12:06:21 AM
**Updated:** 2/16/2026, 12:34:46 AM

---

## User

@src/native/napi.c Sketch me out a fairly simple object parser / validator in the spirit and style of sl_napi_cb_t. We like to use designated initializers to be declarative, but I realize that's difficult with a nested structure.

sl_napi_create is the parsing code we're trying to simplify to start. The rest of the parsing code will use whatever we write.

Keep it simple and keep it declarative. No code yet, just a proposal.

---

## Assistant

I’d do a tiny flat schema parser, not a generic recursive engine.
It stays in the `sl_napi_cb_t` style: descriptor in, result struct out, one pass, first error wins.

- Add `sl_napi_obj_field_desc_t` with:
  - `name` (property key)
  - `required` (or flags)
  - `parse` callback (`napi_value` -> destination slot)
  - `cleanup` callback (for unwind on failure)
  - `offset` into a destination struct (so no nested designated initializer pain)
- Add `sl_napi_obj_desc_t` with:
  - `num_fields`
  - fixed array of field descriptors
  - optional `strict` flag (reject unknown keys) — I’d default this off for now
- Add `sl_napi_obj_t` result with:
  - `error` (`SL_NAPI_*`)
  - `napi_error` (`napi_status`) when relevant
  - failed field index/name for good throw messages

Parse flow (single loop, declarative):

- Validate input is object
- For each declared field: lookup by name, handle missing-required, run field parser into `out + offset`
- Track successfully parsed fields; on failure, run cleanup callbacks in reverse
- Return `sl_napi_obj_t` (no throwing inside parser), so caller decides error message/throw behavior

For `sl_napi_create` in `src/native/napi.c`, the schema would be 3 declarative entries:

- `read`: optional, parse scope array, write to `opts.read`, cleanup `sl_napi_free_scope`
- `write`: optional, parse scope array, write to `opts.write`, cleanup `sl_napi_free_scope`
- `network`: optional, parse bool, write to `opts.network` as `0/1`

Why this works well:

- Keeps call sites short and declarative with designated initializers
- Avoids nested schema complexity by using flat field descriptors + offsets
- Reuses existing parsing helpers (`sl_napi_copy_scope`, bool parser) cleanly
- Gives one consistent pattern for all future object parsers in this file

If you want, next step is implementing just this minimal layer and migrating only `sl_napi_create` first.

---

## User

Show me what a call site would look like

---

## Assistant

Something like this (illustrative only, not implemented):

```c
static napi_value sl_napi_create(napi_env env, napi_callback_info info) {
  napi_value out = SL_NULLPTR;

  sl_napi_cb_t cb = sl_napi_validate_cb(env, info, (sl_napi_cb_desc_t){
    .num_args = 1,
    .types = { napi_object },
  });
  if (cb.error) return SL_NULLPTR;

  sb_opts_t opts = {
    .read = SL_ZERO,
    .write = SL_ZERO,
    .network = 0,
  };

  sl_napi_obj_t parsed = sl_napi_parse_obj(env, cb.args[0], &opts, &(sl_napi_obj_desc_t){
    .num_fields = 3,
    .fields = {
      {
        .name = "read",
        .required = false,
        .offset = offsetof(sb_opts_t, read),
        .parse = sl_napi_parse_scope,
        .cleanup = sl_napi_cleanup_scope,
      },
      {
        .name = "write",
        .required = false,
        .offset = offsetof(sb_opts_t, write),
        .parse = sl_napi_parse_scope,
        .cleanup = sl_napi_cleanup_scope,
      },
      {
        .name = "network",
        .required = false,
        .offset = offsetof(sb_opts_t, network),
        .parse = sl_napi_parse_bool_u8,
        .cleanup = SL_NULLPTR,
      },
    },
  });

  if (parsed.error) {
    sl_napi_throw_parse_error(env, parsed);
    goto done;
  }

  sl_ctx_t* sb = sb_create(&opts);
  if (!sb) goto done;

  // ... same handle/external creation path ...

done:
  sl_napi_free_scope(&opts.write);
  sl_napi_free_scope(&opts.read);
  return out;
}
```

That’s the shape I’d aim for: one declarative field table inline at the call site, no nested parser logic in `sl_napi_create`, and cleanup still obvious.

---

## User

Would sl_napi_parse_scope potentially itself call sl_napi_parse_obj if it had subobjects?

---

## Assistant

Yes, it could.

- If a field parser receives an object value, it can delegate to `sl_napi_parse_obj` for that sub-shape.
- That gives composability: top-level object parser + per-field subobject parsers.

For your current goal, I’d keep `sl_napi_parse_scope` as a leaf parser (array-of-strings) and only use nested `sl_napi_parse_obj` when a field is truly an object with its own schema. That keeps the first version simple while still enabling deeper schemas later.

---

## User

Why not just put pointers instead of offset + struct?

---

## Assistant

You can, and for a tiny one-off parser it’s often simpler.

Why I suggested `offset + base struct` for this pattern:

- It keeps the call site fully declarative: schema says “`network` maps to `sb_opts_t.network`” without predeclaring a bunch of pointer locals.
- It scales better as field count grows; you pass one `&opts` and a table, not N destination pointers threaded through helpers.
- It avoids pointer-type proliferation in descriptor structs (you’d otherwise need per-type pointer handling or `void*` + casts anyway).
- Cleanup is easier to centralize because destination ownership is tied to one output struct.

When direct pointers are better:

- Very small parser (1-3 fields)
- Heterogeneous destinations that are not in one struct
- You want maximum clarity over genericity

So the tradeoff is basically:

- `pointer targets`: simpler mechanics, less generic
- `offset + base`: more declarative and uniform, better for “many object parsers in same style”

Given you want a reusable style for the rest of parsing code, `offset + base` is usually the better long-term fit.

---

## User

Show me what the parse and cleanup for scope look like

---

## Assistant

Here’s the shape I’d use for `scope` as a field parser pair.

```c
typedef s32 (*sl_napi_field_parse_t)(napi_env env, napi_value value, void* dst);
typedef void (*sl_napi_field_cleanup_t)(void* dst);

static s32 sl_napi_parse_scope(napi_env env, napi_value value, void* dst) {
  sl_scope_t* scope = (sl_scope_t*)dst;
  *scope = (sl_scope_t)SL_ZERO;
  return sl_napi_copy_scope(env, value, scope);
}

static void sl_napi_cleanup_scope(void* dst) {
  sl_scope_t* scope = (sl_scope_t*)dst;
  sl_napi_free_scope(scope);
}
```

And in the field descriptor:

```c
{
  .name = "read",
  .required = false,
  .offset = offsetof(sb_opts_t, read),
  .parse = sl_napi_parse_scope,
  .cleanup = sl_napi_cleanup_scope,
}
```

Why this pair is nice:

- Parse is tiny and reuses your existing `sl_napi_copy_scope`.
- Cleanup is idempotent because `sl_napi_free_scope` already handles zero/empty safely.
- Works for both unwind-on-error in the generic parser and normal end-of-function cleanup in `sl_napi_create`.

---

## User

I don't like the unwind thing. Can we make it so that the parser just parses, fills in pointers, and allocates when needed, and then the caller checks failure and calls the free function for the type, which gracefully handles null? You need that free function anyway.

Let's do that. Go ahead and write it. Don't integrate it yet.

---

## Assistant

---

