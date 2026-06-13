# Lookup array values dynamically
idx := 0
arr := [100 20 30]
print arr.(idx)
# => 100

# More complex expressions in the subscript operation
arr.(+ idx 1) = 50
print arr
# => [100 50 30]

# Lookup into a temporary
print [10 20 30].(+ 1 1)
# => 30

# Lookup into a namespace by atom
obj := {}
obj.('hello) = "what's up"
print obj.hello
# => what's up

# More complicated expression again
get-ident := {'foo}
obj.(get-ident()) = 100
print obj.foo
# => 100
