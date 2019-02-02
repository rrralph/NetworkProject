if yyparse() is called multiple times, caution to reset its state by using yyrestart()
[flex](http://dinosaur.compilertools.net/flex/flex_10.html)
```If `yylex()' stops scanning due to executing a return statement in one of the actions, the scanner may then be called again and it will resume scanning where it left off.
```

