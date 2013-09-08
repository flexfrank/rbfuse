# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'rbfuse/version'

Gem::Specification.new do |spec|
  spec.name          = "rbfuse"
  spec.version       = RbFuse::VERSION
  spec.authors       = ["Shumpei Akai"]
  spec.email         = ["flexfrank@gmail.com"]
  spec.description   = %q{A FUSE binding for ruby}
  spec.summary       = %q{}
  spec.homepage      = ""
  spec.license       = "MIT"

  spec.files         = `git ls-files`.split($/)
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ["ext", "lib"]
  spec.extensions    = ["ext/extconf.rb"]

  spec.add_development_dependency "bundler", "~> 1.3"
  spec.add_development_dependency "rake"
end
