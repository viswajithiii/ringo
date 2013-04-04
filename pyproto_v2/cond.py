import pyparsing as pp
import operator as op
import utils

class Condition:
  """
  Class used to filter rows in a table
  """
  def __init__(self,cond):
    self.cond = cond
    self.root = self.parse() # parse the expression to a tree and store the root

  def parse(self):
    col = pp.Word(pp.alphanums)
    val = pp.Word(pp.alphanums)
    comp = pp.Regex('==|!=|<=|>=|<|>')
    cond = pp.Group(col + comp + val)
    expr = pp.operatorPrecedence(cond,[('&&',2,pp.opAssoc.LEFT),('||',2,pp.opAssoc.LEFT)])
    return expr.parseString(self.cond).asList()[0]

  def eval(self,rowdict):
    # NOTE: comparisons with "None" evaluate to True!
    return self.evalnode(self.root,rowdict)
  def evalnodeleft(self,node,rowdict):
    if not isinstance(node,list):
      return rowdict[node]
    return self.evalnode(node,rowdict)
  def evalnoderight(self,node,rowdict):
    if not isinstance(node,list):
      return node
    return self.evalnode(node,rowdict)
  def evalnode(self,node,rowdict):
    err = 'Wrong node format'
    if len(node) != 3:
      raise Exception(err)
    vleft = self.evalnodeleft(node[0],rowdict)
    vright = self.evalnoderight(node[2],rowdict)
    # Convert the given string value to the appropriate type
    if isinstance(vleft,bool) and not isinstance(vright,bool):
      raise Exception(err)
    try:
      if isinstance(vleft,(int,float)):
        vright = float(vright)
      if isinstance(vleft,utils.Date):
        vright = utils.Date(vright)
    except ValueError:
      raise Exception(err)
    return {'&&':op.__and__,
            '||':op.__or__,
            '==':op.eq,
            '!=':op.ne,
            '>=':op.ge,
            '<=':op.le,
            '>':op.gt,
            '<':op.lt}.get(node[1])(vleft,vright)