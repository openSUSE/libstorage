require "storage"

module Storage
  class << self
    def convert_encoding value
      case value
      when String
        value.force_encoding("UTF-8")
      when Hash
        value.keys.each do |key|
          old_val = value.delete[key]
          value[convert_encoding(key)] = convert_encoding(old_val)
        end
        value
      when Array
        value.map { |e| convert_encoding(e) }
      else
        value
      end
    end

    public_instance_methods.each do |method|
      # skip convert method
      next if method == :convert_encoding
      # skip common ruby object methods
      next if Object.methods.include? method
      alias_method (method.to_s + "_unconverted").to_sym, method
      define_method(method) do |*args|
        convert_encoding send((method.to_s + "_unconverted").to_sym, *args)
      end
    end
  end
end
