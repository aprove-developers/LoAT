grammar qepcad;

// entry point
main    :       formula;

// arithmetic expressions
expr    :       VAR | INT | LPAR MINUS expr RPAR | MINUS expr | LPAR expr expr RPAR | expr expr | LPAR expr binop expr RPAR | expr binop expr;
binop   :       EXP | PLUS | MINUS;

// formulas
formula :       BTRUE | BFALSE | lit | LPAR formula boolop formula RPAR | formula boolop formula;
lit     :       LPAR expr relop expr RPAR | expr relop expr;
boolop  :       AND | OR;
relop   :       LT | LEQ | EQ | GT | GEQ | NEQ;

// lexer stuff
PLUS	:	'+';
MINUS	:	'-';
EXP     :       '^';
LPAR 	:	'[';
RPAR	:	']';
AND	:	'/\\';
OR      :       '\\/';
LT	:	'<';
LEQ     :       '<=';
EQ      :       '=';
NEQ     :       '/=';
GEQ     :       '>=';
GT      :       '>';
BTRUE   :       'TRUE';
BFALSE  :       'FALSE';
VAR     :       ('a'..'z'|'A'..'Z'|'_') ('a'..'z'|'A'..'Z'|'0'..'9'|'_'|'.'|'\'')*;
INT     :       '0'..'9'+;
WS      :       (' ' | '\t' | '\r' | '\n')+ -> skip;