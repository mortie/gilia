# We test the print function first,
# because all other tests rely on prints
print "print"
# => print
print none
# => (none)
print 'true 'false
# => (true) (false)
print 100
# => 100
print 100.5
# => 100.5
print 0xff
# => 255
print "Hello World"
# => Hello World
print [none 'true 'false 100.1 "Nope" {} {0}]
# => [(none) (true) (false) 100.1 Nope (namespace) (function)]
print {} {a: 10}
# => (namespace) (namespace)

print "+"
# => +
print +()
# => 0
print +(10)
# => 10
print 10 + 20
# => 30
print (+ 10 20 30)
# => 60

print "-"
# => -
print -() -(10)
# => 0 -10
print 10 - 20
# => -10
print (- 10 20 30)
# => -40

print "/"
# => /
print /() /(10)
# => 1 0.1
print 10 / 2
# => 5
print (/ 10 2 2)
# => 2.5

print "*"
# => *
print *() *(10)
# => 1 10
print 10 * 2
# => 20
print (* 10 20 30)
# => 6000

print "=="
# => ==
print ==() ==(10)
# => (true) (true)
print 10 == 10 20 == 10
# => (true) (false)
print (== 10 20 30)
# => (false)
print (== 'a 'a 'a)
# => (true)

print "!="
# => !=
print !=() !=(10)
# => (false) (false)
print 10 != 10 20 != 10
# => (false) (true)
print (!= 10 20 30)
# => (true)
print (!= 'a 'a 'a)
# => (false)

print ">"
# => >
print 10 > 20
# => (false)
print 20 > 10
# => (true)
print 10 > 10
# => (false)
print (> 1 10 100 200)
# => (false)

print ">="
# => >=
print 10 >= 20
# => (false)
print 20 >= 10
# => (true)
print 10 >= 10
# => (true)
print (>= 1 10 100 200)
# => (false)

print "<"
# => <
print 10 < 20
# => (true)
print 20 < 10
# => (false)
print 10 < 10
# => (false)
print (< 1 10 100 200)
# => (true)

print "<="
# => <=
print 10 <= 20
# => (true)
print 20 <= 10
# => (false)
print 10 <= 10
# => (true)
print (<= 1 10 100 200)
# => (true)

print "len"
# => len
print (len 10)
# => 0
print (len [])
# => 0
print (len [10 20])
# => 2
print (len "Hello")
# => 5
print (len {a: 10; b: 20; c: 30})
# => 3

print "&&"
# => &&
print &&()
# => (true)
print &&('true)
# => (true)
print &&('false)
# => (false)
print 'true && 'true
# => (true)
print (&& 10 == 10 20 == 20 30 != 40)
# => (true)
print 'true && 10
# => (false)

print "||"
# => ||
print ||()
# => (false)
print ||('true)
# => (true)
print ||('false)
# => (false)
print 'true || 'true
# => (true)
print 'true || 'false
# => (true)
print (|| 'false 10 'true)
# => (true)
print (|| 'false 10 'nope)
# => (false)

print "??"
# => ??
print none ?? none ?? 30 ?? 20
# => 30
print (?? 10 none none)
# => 10
print (?? none none "hello")
# => hello
print ??()
# => (none)
