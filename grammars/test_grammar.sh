# usage: test_grammar.sh $GRAMMAR_NAME $START_RULE $TEST_FILE
javac -cp /usr/share/java/antlr4-runtime.jar:$CLASSPATH *.java
/usr/share/antlr4/grun $@
