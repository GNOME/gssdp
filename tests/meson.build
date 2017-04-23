test_deps = [glib_dep, gobject_dep, gio_dep]

functional = executable('test-functional',
                        ['test-functional.c', 'test-util.c'],
                        dependencies : test_deps,
                        include_directories : include_directories('..'),
                        link_with: libgssdp
                        )
regression = executable('test-regression',
                        ['test-regression.c', 'test-util.c'],
                        dependencies : test_deps,
                        include_directories : include_directories('..'),
                        link_with: libgssdp
                        )

# Cannot run tests in parallel because both would do UPnP on localhost
test('Functional test', functional, is_parallel: false)
test('Bug regression test', regression, is_parallel: false)
