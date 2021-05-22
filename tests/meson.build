functional = executable('test-functional',
                        ['test-functional.c', 'test-util.c'],
                        c_args : '-DHAVE_CONFIG_H=1',
                        dependencies : gssdp)

regression = executable('test-regression',
                        ['test-regression.c', 'test-util.c'],
                        c_args : '-DHAVE_CONFIG_H=1',
                        dependencies : gssdp)

# Cannot run tests in parallel because both would do UPnP on localhost
test('Functional test', functional, is_parallel: false)
test('Bug regression test', regression, is_parallel: false)
