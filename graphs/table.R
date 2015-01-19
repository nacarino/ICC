#!/usr/bin/Rscript

# int-tr-j.R
# Simple R script to make graphs from ndnSIM tracers - Satisfied Interests / Timed out interests

#
# Copyright (c) 2014 Waseda University, Sato Laboratory
# Author: Jairo Eduardo Lopez <jairo@ruri.waseda.jp>
#
# int-tr-j.R is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# int-tr-j.R is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of              
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               
# GNU Affero Public License for more details.                                 
#                                                                             
# You should have received a copy of the GNU Affero Public License            
# along with int-tr-j.R.  If not, see <http://www.gnu.org/licenses/>.
#

suppressPackageStartupMessages(library (ggplot2))
suppressPackageStartupMessages(library (scales))
suppressPackageStartupMessages(library (optparse))
suppressPackageStartupMessages(library (doBy))

# set some reasonable defaults for the options that are needed
option_list <- list (
  make_option(c("-f", "--file"), type="character", default="results/aggregate-trace.txt",
              help="File which holds the raw aggregate data.\n\t\t[Default \"%default\"]")
)

# Load the parser
opt = parse_args(OptionParser(option_list=option_list, description="Creates graphs from ndnSIM L3 Data Aggregate Tracer data"))

data = read.table (opt$file, header=T)
data$Node = factor (data$Node)
data$Kilobits <- data$Kilobytes * 8
data$Type = factor (data$Type)

intdata = data

satisfied = subset(intdata, Type %in% c("SatisfiedInterests"))

timedout = subset(intdata, Type %in% c("TimedOutInterests"))

satisfied = subset(satisfied, Node %in% 0)
timedout = subset(timedout, Node %in% 0)

print(sum(satisfied$Packets))
print(sum(timedout$Packets))
