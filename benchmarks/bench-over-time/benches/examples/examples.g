arr := [10 20 30]
print arr

# Lookup into the array with dot-number
print arr.2

# Assign to an index of the array
arr.1 = "nope"
print arr

print []
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
print "If"
if 'true {print "true is true"} {print "true is not true"}
if 'false {print "false is true"} {print "false is not true"}
if 'true {print "true is true"}
if 'false {print "false is true"} # This should output nothing
print (if 'true {"If returns its true value"})
print (if 'false {0} {"If returns its false value"})

print "\nRun loop 10 times"
i := 0
loop {
	print "Hello World"
	i = i + 1
	if i >= 10 {'stop}
}

print "\nWith while this time"
i := 0
while {i < 10} {
	print "Hallo Wrodl"
	i = i + 1
}

print "\nAnd with the for loop"
i := 0
iter := {
	j := i
	i = i + 1
	if j < 10 {j} {'stop}
}
for iter {print "Hello with" $}

print "\nArray iterator"
array-iter := {
	arr := $.0
	idx := 0
	{
		if idx >= (len arr) {
			'stop
		} {
			j := idx
			idx = idx + 1
			[j arr.(j)]
		}
	}
}
for (array-iter ["hello" "world" "how" "are" "you" 10 20 'true]) print
# Lookup array values dynamically
idx := 0
arr := [100 20 30]
print arr.(idx)

# More complex expressions in the subscript operation
arr.(+ idx 1) = 50
print arr

# Lookup into a temporary
print [10 20 30].(+ 1 1)

# Lookup into a namespace by atom
obj := {}
obj.('hello) = "what's up"
print obj.hello

# More complicated expression again
get-ident := {'foo}
obj.(get-ident()) = 100
print obj.foo
# Functions return their last expression
func := {
	"hello"
}

# Call a function without arguments using ()
print func()

# A function, returning a function, with a closure so that it can access
# variables from outside
str := "Hello World"
func := {
	{
		str
	}
}

# func() returns a function, which can be called again with an extra ()
print func()()

func := {
	{
		# $ is a variable containing the arguments passed to the function,
		# so this nested function sets the global 'str' variable to whatever
		# the function is passed
		str = $.0
	}
}

# func() calls the 'func' function with no arguemnts, and the nested function
# is returned. That returned function is then called with 10 as a parameter.
func() 10

# Since the nested function modified it, 'str' should now be the number 10
print str

func := {
	# Variables don't have to be global; this function returns a function which
	# has a reference to the 'retval' variable
	retval := "what's up"
	{retval}
}
print func()()

# Functions can be called infix
print 10 + 20

# With complex expressions as left and right
print ({$.0 + $.1} 10 20) + (50 - 40)

# Infix operators are evaluated left to right
print 10 + 20 - 5

# Custom functions starting with a dollar can be infix
$shift-add := {$.0 * 10 + $.1}
print 1 $shift-add 2

# Lastly, just print a function literal
print {0}
obj := {
	foo: "hello world"
}
print obj.foo

# Assignments
obj.foo = 100
print obj.foo

# Nested namespaces should work
obj.bar = {baz: "how's your day going?"}
print obj.bar.baz

# Empty namespace literal (should parse to namespace, not function)
print {}
