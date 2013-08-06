
require 'rubygems'
require 'json'

require 'csv'

library = JSON.parse( IO.read(ARGV[0]) )
CSV.open("library_contents.csv", "wb") do |csv|
	csv << ["artist", "track"]
	library.each do |record|
		csv << [record["metadata"]["artist"], record["metadata"]["title"]]
	end
end