# usage: test_grammar.sh $GRAMMAR_NAME $START_RULE $TEST_FILE
javac -cp /usr/share/java/antlr4-runtime.jar:$CLASSPATH *.java
GRAMMAR=$1
ENTRY=$2
shift 2
/usr/share/antlr4/grun $GRAMMAR $ENTRY -gui $@
