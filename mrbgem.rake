MRuby::Gem::Specification.new 'mruby-bin-picorbc' do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'picoruby compiler executable'
  spec.add_dependency 'mruby-pico-compiler', github: 'hasumikin/mruby-pico-compiler'

  exec = exefile("#{build.build_dir}/bin/picorbc")

  picorbc_obj = objfile("#{dir}/tools/picorbc/picorbc.c".pathmap("#{build_dir}/tools/picorbc/%n"))

  pico_compiler_srcs = %w(common compiler context dump generator mrbgem my_regex
                          node regex scope stream token tokenizer)
  pico_compiler_objs = pico_compiler_srcs.map do |name|
    objfile("#{build.gems['mruby-pico-compiler'].build_dir}/src/#{name}")
  end

  ptr_size = "#{build.gems['mruby-pico-compiler'].clone.dir}/include/ptr_size.h"

  parse_h = "#{build.gems['mruby-pico-compiler'].clone.dir}/include/parse.h"

  pico_compiler_objs << picorbc_obj

  file exec => [ptr_size, parse_h, picorbc_obj] + pico_compiler_objs do |t|
    build.linker.run t.name, pico_compiler_objs
  end

  build.bins << 'picorbc'
end
