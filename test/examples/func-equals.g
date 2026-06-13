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
