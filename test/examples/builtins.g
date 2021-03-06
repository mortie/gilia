print "+"
print +()
print +(10)
print 10 + 20
print (+ 10 20 30)

print "\n-"
print -() -(10)
print 10 - 20
print (- 10 20 30)

print "\n/"
print /() /(10)
print 10 / 2
print (/ 10 2 2)

print "\n*"
print *() *(10)
print 10 * 2
print (* 10 20 30)

print "\n=="
print ==() ==(10)
print 10 == 10 20 == 10
print (== 10 20 30)
print (== 'a 'a 'a)

print "\n!="
print !=() !=(10)
print 10 != 10 20 != 10
print (!= 10 20 30)
print (!= 'a 'a 'a)

print "\n>"
print 10 > 20
print 20 > 10
print 10 > 10
print (> 1 10 100 200)

print "\n>="
print 10 >= 20
print 20 >= 10
print 10 >= 10
print (>= 1 10 100 200)

print "\n<"
print 10 < 20
print 20 < 10
print 10 < 10
print (< 1 10 100 200)

print "\n<="
print 10 <= 20
print 20 <= 10
print 10 <= 10
print (<= 1 10 100 200)

print "\nprint"
print none
print 'true 'false
print 100
print 100.5
print 0xff
print "Hello World"
print [none 'true 'false 100.1 "Nope" {} {0}]
print {} {a: 10}

print "\nlen"
print (len 10)
print (len [])
print (len [10 20])
print (len "Hello")
print (len {a: 10; b: 20; c: 30})

print "\n&&"
print &&()
print &&('true)
print &&('false)
print 'true && 'true
print (&& 10 == 10 20 == 20 30 != 40)
print 'true && 10

print "\n||"
print ||()
print ||('true)
print ||('false)
print 'true || 'true
print 'true || 'false
print (|| 'false 10 'true)
print (|| 'false 10 'nope)

print "\n??"
print none ?? none ?? 30 ?? 20
print (?? 10 none none)
print (?? none none "hello")
print ??()
