; Highlights for the Axiom model/query language (`.axm`).

(line_comment) @comment

("import" @keyword)
("from" @keyword)
("as" @keyword)
("type" @keyword)
("model" @keyword)
("extends" @keyword)
("select" @keyword)
("query" @keyword)
("true" @keyword)
("false" @keyword)

(string) @string
(number) @number

(imported_name
  name: (identifier) @type)

(imported_name
  alias: (identifier) @type)

(type_decl
  name: (identifier) @type)

(type_ref
  (identifier) @type)

(model_decl
  name: (identifier) @type)

(model_source
  (db_ident) @label)

(field_decl
  name: (identifier) @property)

(param_decl
  name: (identifier) @variable.parameter)

(query_decl
  name: (identifier) @function)

(call
  method: (identifier) @function.call)