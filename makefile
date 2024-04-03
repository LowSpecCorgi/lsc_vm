# targets:prerequisites

source_folder := src
build_folder := build
files := $(wildcard $(source_folder)/*)
output_file := $(build_folder)/lsc_vm.exe

# Source files
lsc_vm: $(files)
	gcc $(files) -o $(output_file)

run: $(output_file)
	echo Running project.
	./$(output_file)

clean: $(output_file)
	echo cleaning output file: $(output_file)
	rm $(output_file)