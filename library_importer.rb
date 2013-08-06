
require 'rubygems'
require 'json'

require "net/http"

library = JSON.parse( IO.read(ARGV[0]) )
library.each do |record|
	uri = URI('http://ec2-50-16-103-27.compute-1.amazonaws.com:8080/ingest')
	res = Net::HTTP.post_form(uri, "fp_code" => record["code"],
					"codever" => "4.12",
					"track" => record["metadata"]["title"],
					"artist" => record["metadata"]["artist"],
					"length" => record["metadata"]["duration"])
	puts res.body.inspect
end	