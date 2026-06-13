print "if"
# => if
if 'true {print "true is true"} {print "true is not true"}
# => true is true
if 'false {print "false is true"} {print "false is not true"}
# => false is not true
if 'true {print "true is true"}
# => true is true
if 'false {print "false is true"} # This should output nothing
print (if 'true {"If returns its true value"})
# => If returns its true value
print (if 'false {0} {"If returns its false value"})
# => If returns its false value

print "Run loop 5 times"
# => Run loop 5 times
i := 0
loop {
	print "Hello World"
	i = i + 1
	if i >= 5 {'stop}
}
# => Hello World
# => Hello World
# => Hello World
# => Hello World
# => Hello World

print "With while this time"
# => With while this time
i := 0
while {i < 5} {
	print "Hallo Wrodl"
	i = i + 1
}
# => Hallo Wrodl
# => Hallo Wrodl
# => Hallo Wrodl
# => Hallo Wrodl
# => Hallo Wrodl

print "And with the for loop"
# => And with the for loop
i := 0
iter := {
	j := i
	i = i + 1
	if j < 5 {j} {'stop}
}
for iter {print "Hello with" $}
# => Hello with [0]
# => Hello with [1]
# => Hello with [2]
# => Hello with [3]
# => Hello with [4]

print "Array iterator"
# => Array iterator
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
# => [0 hello]
# => [1 world]
# => [2 how]
# => [3 are]
# => [4 you]
# => [5 10]
# => [6 20]
# => [7 (true)]
