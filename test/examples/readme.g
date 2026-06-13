print "Hello World"
# => Hello World

print "Hello World"
print "Goodbye World"
print "See You"
# => Hello World
# => Goodbye World
# => See You

i := 10
if i < 20 {
	print "i is less than 20"
}
# => i is less than 20

i := 0
while {i < 5} {
	print i
	i += 1
}
# => 0
# => 1
# => 2
# => 3
# => 4

is-n := |n| {{$.0 == n}}
is-any := {'true}

match 24
-> is-n(10) {print "It was 10"}
-> is-n(20) {print "It was 20"}
-> is-any |x| {print "It was neither:" x}
# => It was neither: 24

alice := {
	name: "Alice"
	age: 32
	profession: "Programming Language Developer"
}
print alice.name
# => Alice

numbers := [10 20 30]
print numbers.0
# => 10

numbers := [10 20 30 40]
get-value := |base offset| {numbers.(base + offset)}
print (get-value 1 2)
# => 40

alice := {
	name: "Alice"
	age: 32
	profession: "Programming Language Developer"
}
get-field := |field| {alice.(field)}
print (get-field 'age)
# => 32

values := [10 20 30 40]
index := 0
iter := {
	guard index >= len(values) {'stop}
	ret := [index values.(index)]
	index += 1
	ret
}
for iter print
# => [0 10]
# => [1 20]
# => [2 30]
# => [3 40]
