require 'rubygems'
require 'rake'

begin
  require 'jeweler'
  Jeweler::Tasks.new do |gem|
    gem.name = "rbfuse"
    gem.summary = %Q{fuse binding}
    gem.description = %Q{fuse binding for ruby}
    gem.email = "flexfrank@gmail.com"
    gem.homepage = "http://github.com/flexfrank/rbfuse"
    gem.authors = ["Shumpei Akai"]
    gem.add_development_dependency "rspec", ">= 1.2.9"
    gem.extensions=["ext/extconf.rb"]
    gem.files.include 'lib/**/*.rb'
    gem.files.include 'ext/**/*.h'
    gem.files.include 'ext/**/*.c'
    # gem is a Gem::Specification... see http://www.rubygems.org/read/chapter/20 for additional settings
  end
  Jeweler::GemcutterTasks.new
rescue LoadError
  puts "Jeweler (or a dependency) not available. Install it with: gem install jeweler"
end
=begin
require 'spec/rake/spectask'
Spec::Rake::SpecTask.new(:spec) do |spec|
  spec.libs << 'lib' << 'spec'
  spec.spec_files = FileList['spec/**/*_spec.rb']
end

Spec::Rake::SpecTask.new(:rcov) do |spec|
  spec.libs << 'lib' << 'spec'
  spec.pattern = 'spec/**/*_spec.rb'
  spec.rcov = true
end
task :spec => :check_dependencies

task :default => :spec
=end
require 'rake/rdoctask'
Rake::RDocTask.new do |rdoc|
  version = File.exist?('VERSION') ? File.read('VERSION') : ""

  rdoc.rdoc_dir = 'rdoc'
  rdoc.title = "rbfuse #{version}"
  rdoc.rdoc_files.include('README*')
  rdoc.rdoc_files.include('lib/**/*.rb')
end
