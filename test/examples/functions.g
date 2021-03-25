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
