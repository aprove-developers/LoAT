grammar Koat;

// entry point
main    :       goal? start vardecl transs;

// aliases for ID
fs      :       ID;
var     :       ID;

// top-level elements
goal    :       LPAR GOAL (CPX | TERM) RPAR;
start   :       LPAR START LPAR FS fs RPAR RPAR;
vardecl :       LPAR VAR ID+ RPAR;
transs  :       LPAR RULES trans* RPAR;

// transitions
trans   :       lhs to rhs cond?;
lhs     :       fs LPAR (var (COMMA var)*)? RPAR;
rhs     :       fs LPAR (expr (COMMA expr)*)? RPAR;
to      :       TO | MINUS LCURL lb COMMA ub RCURL GT | MINUS LCURL lb RCURL GT;
lb      :       expr;
ub      :       expr;
cond    :       CONDSEP formula | LBRACK formula RBRACK;

// arithmetic expressions
expr    :       var | INT | MINUS expr | expr binop expr | LPAR expr RPAR;
binop   :       EXP | TIMES | PLUS | MINUS;

// formulas
formula :       lit | formula boolop formula | LPAR formula RPAR;
lit     :       expr relop expr;
boolop  :       AND | OR;
relop   :       LT | LEQ | EQ | GT | GEQ | NEQ;

// lexer stuff
GOAL    :       'GOAL';
CPX     :       'COMPLEXITY';
TERM    :       'TERMINATION';
START   :       'STARTTERM';
FS      :       'FUNCTIONSYMBOLS';
VAR     :       'VAR';
RULES   :       'RULES';
PLUS	:	'+';
MINUS	:	'-';
TIMES	:	'*';
EXP     :       '^' | '**';
LPAR 	:	'(';
RPAR	:	')';
RBRACK	:	']';
LBRACK 	:	'[';
LCURL   :       '{';
RCURL   :       '}';
TO	:	'->';
COMMA	:	',';
AND	:	'/\\' | '&&';
OR      :       '\\/' | '||';
LT	:	'<';
LEQ     :       '<=';
EQ      :       '==';
NEQ     :       '!=';
GEQ     :       '>=';
GT      :       '>' ;
CONDSEP :       ':|:';
ID      :       ('a'..'z'|'A'..'Z'|'_') ('a'..'z'|'A'..'Z'|'0'..'9'|'_'|'.'|'\'')*;
INT     :       '0'..'9'+;
WS      :       (' ' | '\t' | '\r' | '\n')+ -> skip;
COMMENT :       '#' .*? ('\r'? '\n' | '\r') -> skip;