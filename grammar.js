// A tree-sitter grammar for the Axiom model/query language (.axm files).
//
// Mirrors the hand-rolled parser in `crates/axiom-core/src/axm/parser.rs`:
// imports, type aliases, models (with `extends select<...>` sources) and
// query declarations whose body is a raw, brace-balanced SQL snippet handled
// by the external scanner in `src/scanner.c`.

module.exports = grammar({
  name: 'axm',

  extras: ($) => [/\s/, $.line_comment],

  word: ($) => $.identifier,

  externals: ($) => [$.sql_body],

  rules: {
    // A file is a sequence of declarations with no separators required
    // between them; every item consumes its own leading/trailing trivia.
    source_file: ($) => repeat($._item),

    _item: ($) =>
      choice($.import_stmt, $.type_decl, $.model_decl, $.query_decl),

    // Identifiers are NOT word-bounded: `importX` must lex as one identifier
    // rather than the `import` keyword, so keywords are matched by maximal
    // munch (`import` only wins when no word character follows).
    identifier: ($) => /[a-zA-Z_][a-zA-Z0-9_]*/,

    line_comment: ($) => token(seq('//', /[^\n\r]*/)),

    // Double-quoted string literal. `\` escapes any following character, so
    // the escape arm is `\\` + any char; unterminated strings become ERROR.
    string: ($) =>
      token(
        seq(
          '"',
          repeat(choice(/[^"\\]+/, seq('\\', /./))),
          '"',
        ),
      ),

    number: ($) => /[0-9]+(\.[0-9]+)?/,

    literal: ($) => choice($.string, $.number, 'true', 'false'),

    // import { a, b as c, } from "schema";
    import_stmt: ($) =>
      seq(
        'import',
        '{',
        repeat(seq($.imported_name, optional(','))),
        '}',
        'from',
        field('source', $.string),
        optional(';'),
      ),

    imported_name: ($) =>
      seq(field('name', $.identifier), optional(seq('as', field('alias', $.identifier)))),

    // @safeParse("first"), @parse, @target("typescript", "rust")
    decorator: ($) =>
      seq(
        '@',
        field('name', $.identifier),
        optional(
          seq(
            '(',
            repeat(seq($.literal, optional(','))),
            ')',
          ),
        ),
      ),

    // type Email = String.min_length(3, "...").lowercase();
    type_decl: ($) =>
      seq('type', field('name', $.identifier), '=', $.annotated_type, optional(';')),

    // model User extends select<public.users> { id: UUID, email: Email?; }
    model_decl: ($) =>
      seq(
        repeat($.decorator),
        'model',
        field('name', $.identifier),
        optional($.model_source),
        '{',
        repeat(seq($.field_decl, optional(','))),
        '}',
      ),

    model_source: ($) => seq('extends', 'select', '<', $.db_ident, '>'),

    // Field names may be identifiers or double-quoted strings and may carry
    // a `?` optional marker before the type annotation, e.g. `"two words"? : Int`.
    field_decl: ($) =>
      seq(
        field('name', choice($.identifier, $.string)),
        optional('?'),
        ':',
        $.annotated_type,
        optional(seq('=', $.literal)),
      ),

    // query findUser($id: UUID, name: String?) -> [User] { SELECT * FROM users };
    query_decl: ($) =>
      seq(
        repeat($.decorator),
        'query',
        field('name', $.identifier),
        '(',
        repeat(seq($.param_decl, optional(','))),
        ')',
        optional(seq('->', $.type_ref)),
        $.sql_body,
      ),

    param_decl: ($) =>
      seq(optional('$'), field('name', $.identifier), ':', $.type_ref),

    // Base type with `?` (nullable) and `[]` (array) postfixes.
    type_ref: ($) =>
      seq($.identifier, repeat(choice('?', '[]'))),

    // Chained rule/transform calls: `.trim()`, `.email()`, `.min(1, "...")`, ...
    annotated_type: ($) => seq($.type_ref, repeat($.call)),

    call: ($) =>
      seq(
        '.',
        field('method', $.identifier),
        optional(seq('(', repeat(seq($.literal, optional(','))), ')')),
      ),

    // A database identifier inside `select<...>`, which may be schema
    // qualified (`public.users`) and follows the SQL dialect's charset.
    db_ident: ($) => /[a-zA-Z0-9_.]+/,
  },
});