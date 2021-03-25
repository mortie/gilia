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
