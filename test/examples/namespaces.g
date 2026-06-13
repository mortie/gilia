obj := {
	foo: "hello world"
}
print obj.foo
# => hello world

# Assignments
obj.foo = 100
print obj.foo
# => 100

# Nested namespaces should work
obj.bar = {baz: "how's your day going?"}
print obj.bar.baz
# => how's your day going?

# Empty namespace literal (should parse to namespace, not function)
print {}
# => (namespace)
