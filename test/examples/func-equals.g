# We should be able to use 'a x= b' as a short-hand for 'a = x a b'
add := |a b| { a + b }
num := 10
num add= 20
print num
# => 30
num -= 5
print num
# => 25

# Same is true for namespaces
a := {foo: 10}
a.foo += 20
print a.foo
# => 30

# And arrays
a := [10 20 30]
a.1 -= 5
print a.1
# => 15

# And dynamic lookup
a := [10 20 30]
a.(2 - 1) *= 3
print a.1
# => 60

# ... For namespaces too
a := {bob: 3; alice: 10}
a.('alice) /= 2
print a.alice
# => 5

# And nesting!
a := {
	b: [
		{c: 1}
	]
}
a.('b).(0).c += 3
print a.b.0.c
# => 4
