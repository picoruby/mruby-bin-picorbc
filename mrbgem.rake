MRuby::Gem::Specification.new 'mruby-bin-picorbc' do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby developers'
  spec.summary = 'picoruby compiler executable'
  spec.add_dependency 'mruby-pico-compiler'

  spec.cc.include_paths << "#{build_dir}/../mruby-pico-compiler/include"

  exec = exefile("#{build.build_dir}/bin/picorbc")
  picorbc_objs = Dir.glob("#{spec.dir}/tools/picorbc/*.c").map do
    |f| objfile(f.pathmap("#{spec.build_dir}/tools/picorbc/%n"))
  end

  pico_compiler_objs = Dir.glob("#{spec.build_dir}/../mruby-pico-compiler/src/*.o")
  pico_compiler_objs.reject! { |o| o.split("/").last == "parse.o" }

  file exec => picorbc_objs + pico_compiler_objs do |t|
    build.linker.run t.name, t.prerequisites
  end

  build.bins << 'picorbc'
end
