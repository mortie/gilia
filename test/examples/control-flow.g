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
