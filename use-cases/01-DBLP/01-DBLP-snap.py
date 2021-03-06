"""
 Use case #1: create a coauthorship network and print its estimated diameter
"""

import sys
sys.path.append("..")
import os
import time
import snap
import testutils

N_TEST_NODES = 10
ENABLE_TIMER = True
OUTPUT_TABLE_FILENAME = 'table.tsv'
OUTPUT_GRAPH_FILENAME = 'graph'

if len(sys.argv) < 2:
  print """Usage: python 01-DBLP-snap.py <authors.tsv> <year.tsv> <outputdir>
  authors.tsv: path to authors.tsv file
  year.tsv: path to year.tsv file
  outputdir: output directory (for saving the table of edges and the coauthorship network)"""
  exit(1)
authorFile = sys.argv[1]
yearFile = sys.argv[2]
dstDir = sys.argv[3] if len(sys.argv) >= 4 else None
if not dstDir is None:
  try:
    os.makedirs(dstDir)
  except OSError:
    pass

context = snap.TTableContext()

t = testutils.Timer(ENABLE_TIMER)

# Load data
# >>> authors = ringo.load('authors.tsv')
S = snap.Schema()
S.Add(snap.TStrTAttrPr("Key", snap.atStr))
S.Add(snap.TStrTAttrPr("Author", snap.atStr))
authors = snap.TTable.LoadSS("1", S, authorFile, context, '\t', snap.TBool(False))
t.show("load author table", authors)

# >>> year = ringo.load('year.tsv')
S = snap.Schema()
S.Add(snap.TStrTAttrPr("Key", snap.atStr))
S.Add(snap.TStrTAttrPr("Year", snap.atInt))
year = snap.TTable.LoadSS("2", S, yearFile, context, '\t', snap.TBool(False))
t.show("load year table", year)

# Select
# >>> year.select('Year >= 2005')
year.SelectAtomicIntConst("Year", 2005, snap.GTE)
t.show("select", year)

# Join
# >>> table = authors.join(year, ['Key'], ['Key'])
table = authors.Join("Key", year, "Key")
t.show("join", table)

# Self-join
# >>> table.selfjoin(['Key'])
table = table.SelfJoin("Key")
t.show("join", table)

# Save final table
# >>> table.save('table.tsv')
if not dstDir is None:
  table.SaveSS(os.path.join(dstDir,OUTPUT_TABLE_FILENAME))
  t.show("save edge table", table)

# Create network
# >>> graph = table.graph('Author_1', 'Author_2', directed=False)
# TODO: use simpler conventions for column renaming
table.SetSrcCol("1_2_1.1.Author")
table.SetDstCol("1_2_2.1.Author")
graph = snap.ToGraph(table, snap.aaFirst)
t.show("graph", graph)

if not dstDir is None:
  graph.Save(snap.TFOut(os.path.join(dstDir,OUTPUT_GRAPH_FILENAME)))
  t.show("save graph", graph)

# Print diameter
# >>> print graph.diameter(10000)
diameter = snap.GetBfsEffDiam(graph,N_TEST_NODES)
t.show("diameter (%d test nodes)" % N_TEST_NODES)

print "Diameter: {0:.5f}".format(diameter)
